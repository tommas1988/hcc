#define _XOPEN_SOURCE 500       /* required by nftw */
#define _POSIX_C_SOURCE 200112L /* required by posix_fadvise */

#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>
#include <ftw.h>

#include "ini.h"

#include "error.h"
#include "sq_list.h"
#include "hash.h"
#include "hcc.h"

#include "comment_defs_string.c"

static boolean show_comment_defs = FALSE;
static boolean verbose = FALSE;
static int exclude_list_size = 0;
static struct {
  int lang;
  int pattern;
  int comment;
  int filename;
} field_width;

#ifdef DEBUG
static boolean debug = FALSE;
#endif

static struct hash_table *lang_comment_table;
static struct sq_list lang_pattern_list;
static struct sq_list line_counter_list;
static struct sq_list exclude_list;

static struct sq_list *find_comment_list(const char *filename, char **lang) {
  struct lang_match_pattern *lang_pattern;

  list_reset(&lang_pattern_list);
  while ((lang_pattern = (struct lang_match_pattern *) list_current(&lang_pattern_list))) {
    if (0 == fnmatch(lang_pattern->pattern, filename, 0)) {
      struct sq_list *comment_list;

      comment_list = (struct sq_list *) hash_table_find(lang_comment_table, lang_pattern->lang);
      if (!comment_list) {
        error(EXIT_FAILURE, "Cannot find language: %s comment list", lang_pattern->lang);
      }

      *lang = lang_pattern->lang;

      return comment_list;
    }

    list_next(&lang_pattern_list);
  }

  return NULL;
}

enum {
  COUNTER_COMMENT,
  COUNTER_BLANK,
  COUNTER_CODE,
};

#define update_counter(type, counter)           \
  do {                                          \
    switch (type) {                             \
    case COUNTER_COMMENT:                       \
      (counter)->comment_lines++;               \
      break;                                    \
    case COUNTER_BLANK:                         \
      (counter)->blank_lines++;                 \
      break;                                    \
    case COUNTER_CODE:                          \
      (counter)->code_lines++;                  \
      break;                                    \
    }                                           \
  } while (0)

static void count_line(int fd, struct sq_list *comment_list, struct line_counter *counter) {
  char buf[MAX_COMMENT_SIZE + BUFFER_SIZE], *read_buf;
  ssize_t bytes_read, pos;
  boolean in_code = FALSE, in_comment = FALSE, end_comment = FALSE;
  struct comment *cp;

#ifdef DEBUG
  int line_start_pos = 0;
  int incomplete_line_len = 0;
  char *incomplete_line_buf = NULL;

#define print_scan_line(C)                                              \
  do {                                                                  \
    if (debug) {                                                        \
      putchar(C);                                                       \
      if (incomplete_line_len) {                                        \
        if (!fwrite(incomplete_line_buf, 1, incomplete_line_len, stdout)) { \
          error(EXIT_FAILURE, "Cannot write incomplete line buffer");   \
        }                                                               \
        incomplete_line_len = 0;                                        \
      }                                                                 \
      if (!fwrite(read_buf + line_start_pos, 1, pos - line_start_pos + 1, stdout)) { \
        error(EXIT_FAILURE, "Cannot print debug line");                 \
      }                                                                 \
      line_start_pos = pos + 1;                                         \
    }                                                                   \
  } while (0)
#else
#define print_scan_line(c)
#endif

  read_buf = &buf[MAX_COMMENT_SIZE];
  while ((bytes_read = read(fd, read_buf, BUFFER_SIZE))) {
    if (bytes_read == -1) {
      error(EXIT_FAILURE, "Read file %s error", counter->filename);
    }

    pos = pos < 0 ? pos : 0;
    while (pos < bytes_read) {
      if (in_code) {
        if (read_buf[pos] == '\n') {
          update_counter(COUNTER_CODE, counter);
          /* set in_code to FALSE in order to test next line type */
          in_code = FALSE;

          print_scan_line(' ');
        }
        pos++;
      } else if (in_comment) {
        if (cp->end.len && cp->end.val[0] == read_buf[pos]) { /* comment block and first end comment char is match with current pos */
          int bytes_left = bytes_read - pos;
          int len = bytes_left < cp->end.len ? bytes_left : cp->end.len;

          if (!strncmp(cp->end.val, read_buf + pos, len)) {
            if (bytes_left < cp->end.len) { /* partial match */
#ifdef DEBUG
              {
                char buf[MAX_COMMENT_SIZE];
                struct comment_str *str = &cp->end;

                strncpy(buf, str->val, str->len);
                buf[str->len] = '\0';

                printf("Partial match with %s\n", buf);
              }
#endif
              strncpy(read_buf - len, read_buf + pos, len);
              pos = -len;

              break;            /* refill buffer */
            }

            end_comment = TRUE;
            pos += len;
          } else {
            pos++;
          }
        } else if (read_buf[pos] == '\n') {
          update_counter(COUNTER_COMMENT, counter);

          if (end_comment) {
            in_comment = end_comment = FALSE;
          } else if (!cp->end.len) {   /* inline comment */
            in_comment = FALSE;
          }

          print_scan_line('C');

          pos++;
        } else if (!isspace(read_buf[pos])) { /* not a space charactor */
          if (end_comment) {                  /* when non-space charactor follows then end of comment, re-check */
            in_comment = end_comment = FALSE;
          } else {
            pos++;
          }
        } else {
          pos++;
        }
      } else {
        if (read_buf[pos] == '\n') {
          update_counter(COUNTER_BLANK, counter);

          print_scan_line('B');

          pos++;
        } else if (!isspace(read_buf[pos])) { /* not space charactor */
          int bytes_left = bytes_read - pos, bytes_match = 0;

          list_reset(comment_list);
          while ((cp = (struct comment *) list_current(comment_list))) {
            int len = bytes_left < cp->start.len ? bytes_left : cp->start.len;

            if (!strncmp(cp->start.val, read_buf + pos, len)) {
              bytes_match = len;
              if (bytes_left < cp->start.len) { /* partial match */
#ifdef DEBUG
                {
                  char buf[MAX_COMMENT_SIZE];
                  struct comment_str *str = &cp->start;

                  strncpy(buf, str->val, str->len);
                  buf[str->len] = '\0';

                  printf("Partial match with %s\n", buf);
                }
#endif
                strncpy(read_buf - len, read_buf + pos, len);
                pos = -len;
              } else {
                in_comment = TRUE;
              }

              break;
            }
            list_next(comment_list);
          }

          if (!in_comment) {
            if (pos < 0) {      /* partial match, refill buffer */
              break;
            } else {
              in_code = TRUE;
            }
          }

          pos += (bytes_match ? bytes_match : 1);
        } else {                /* is white space charactor */
          pos++;
        }
      }
    }

#ifdef DEBUG
    if (debug) {
      if (read_buf[pos] != '\n') {
        int len = pos - line_start_pos + 1;
        char *buf;

        if (incomplete_line_len) {
          if (!(incomplete_line_buf = realloc(incomplete_line_buf, incomplete_line_len + len))) {
            error(EXIT_FAILURE, "Cannot realloc incomplete line buffer");
          }

          buf = incomplete_line_buf + incomplete_line_len;
        } else {
          if (incomplete_line_buf && incomplete_line_len < len) {
            free(incomplete_line_buf);
            incomplete_line_buf = NULL;
          }

          if (!incomplete_line_buf) {
            if(!(incomplete_line_buf = malloc(len))) {
              error(EXIT_FAILURE, "Cannot alloc incomplete line buffer");
            }
          }

          buf = incomplete_line_buf;
        }

        memcpy(buf, read_buf + line_start_pos, len);
        incomplete_line_len += len;
      }

      line_start_pos = 0;
    }
#endif
  }

#ifdef DEBUG
  if (incomplete_line_buf) {
    free(incomplete_line_buf);
  }
#endif
}

static void scan_file(const char *filename) {
  int fd;
  char *lang, *pathname, *exclude_pattern;
  int len;
  struct sq_list *comment_list;
  struct line_counter *counter;

  list_reset(&exclude_list);
  while ((exclude_pattern = (char *) list_current(&exclude_list))) {
    if (!fnmatch(exclude_pattern, filename, 0)) {
      return;
    }
    list_next(&exclude_list);
  }

  if (!(comment_list = find_comment_list(filename, &lang))) {
    if (verbose) fprintf(stderr, "No matched language found, skip count file: %s\n", filename);
    return;
  }

  if (!(counter = malloc(sizeof(struct line_counter)))) {
    error(EXIT_FAILURE, "Cannot alloc line_counter");
  }

  len = strlen(filename);
  if (!(pathname = malloc(len + 1))) {
    error(EXIT_FAILURE, "Cannot alloc pathname");
  }

  if (verbose) {
    field_width.filename = len;
  }

  strncpy(pathname, filename, len);
  pathname[len] = '\0';
  counter->filename = pathname;
  counter->lang = lang;
  counter->blank_lines = 0;
  counter->code_lines = 0;
  counter->comment_lines = 0;

  list_append(&line_counter_list, (void *) counter);

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    error(EXIT_FAILURE, "Cannot open file: %s", filename);
  }

  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  count_line(fd, comment_list, counter);

  close(fd);
}

static int count_for_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  if (typeflag == FTW_F) {
    scan_file(fpath);
  }
  return 0;
}

#define lang_str_cpy(dest, lang_str)                    \
  do {                                                  \
    (dest) = malloc(MAX_LANG_SIZE + 1);                 \
    if (!(dest)) {                                      \
      error(EXIT_FAILURE, "Cannot alloc lang key");     \
    }                                                   \
                                                        \
    strncpy((dest), (lang_str), MAX_LANG_SIZE + 1);     \
  } while (0)

static void create_comment_list(struct bucket *bktp, const char *key) {
  char *lang_key;
  struct sq_list *comment_list;

  if (!(comment_list = malloc(sizeof(struct sq_list)))) {
    error(EXIT_FAILURE, "Cannot alloc comment list");
  }

  init_sq_list(comment_list, INIT_LANG_COMMENT_LIST_SIZE);

  lang_str_cpy(lang_key, key);
  bktp->key = lang_key;
  bktp->value = comment_list;
}

static struct comment *create_comment_from_string(const char *str) {
  struct comment *comment;
  int str_len;
  char *cpy, *p;

  str_len = strlen(str);

  if (!(cpy = malloc(str_len))) {
    error(EXIT_FAILURE, "Cannot alloc comment string");
  }

  if (!(comment = malloc(sizeof(struct comment)))) {
    error(EXIT_FAILURE, "Cannot alloc comment");
  }

  strncpy(cpy, str, str_len);

  /* TODO: trim leading & trailing space charactors first */

  p = strchr(cpy, ' ');
  if (p) {
    int start_len = p - cpy;

    comment->start.len = start_len;
    comment->start.val = cpy;
    comment->end.len = str_len - start_len - 1;
    comment->end.val = p + 1;
  } else {
    comment->start.len = str_len;
    comment->start.val = cpy;
    comment->end.len = 0;
    comment->end.val = NULL;
  }

  return comment;
}

#define strtolower(str)                         \
  do {                                          \
    char *p = str;                              \
    while ((*p)) {                              \
      (*p) = tolower((*p));                     \
      p++;                                      \
    }                                           \
  } while (0)

static int build_comment_def(void* unused, const char* lang, const char* name, const char* value) {
  char clang[MAX_LANG_SIZE + 1];

  strncpy(clang, lang, MAX_LANG_SIZE);
  clang[MAX_LANG_SIZE] = '\0';

  strtolower(clang);

  if (show_comment_defs) {
    field_width.lang = strlen(clang);
  }

  if (!strcmp(name, "pattern")) {
    struct lang_match_pattern *lang_pattern;
    char *buf, *pattern;
    int len;

    if (!(lang_pattern = malloc(sizeof(struct lang_match_pattern)))) {
      error(EXIT_FAILURE, "Cannot alloc lang match pattern");
    }

    len = strlen(value);
    if (!(pattern = malloc(sizeof(len + 1)))) {
      error(EXIT_FAILURE, "Cannot alloc pattern string");
    }

    lang_str_cpy(buf, clang);
    strncpy(pattern, value, len);
    pattern[len] = '\0';

    lang_pattern->lang = buf;
    lang_pattern->pattern = pattern;

    list_append(&lang_pattern_list, lang_pattern);

    if (show_comment_defs) {
      field_width.pattern = len;
    }
  } else if (!strcmp(name, "comment")) {
    struct sq_list *comment_list;

    comment_list = (struct sq_list *) hash_table_find_with_add(lang_comment_table, clang, create_comment_list);
    list_append(comment_list, (void *) create_comment_from_string(value));

    if (show_comment_defs) {
      field_width.comment = strlen(value);
    }
  } else {
    error(EXIT_FAILURE, "Unknown comment definition field name");
  }

  return 1;
}

static void display_comment_defs_detail() {
  struct lang_match_pattern *lang_pattern;
  int lang_width, pattern_width, comment_width;
  char *format;

  lang_width = (sizeof("LANGUAGE") > field_width.lang ? sizeof("LANGUAGE") : field_width.lang) + GAP_WIDTH;
  pattern_width = (sizeof("PATTERN") > field_width.pattern ? sizeof("PATTERN") : field_width.pattern) + GAP_WIDTH;
  comment_width = sizeof("COMMENT") > field_width.comment ? sizeof("COMMENT") : field_width.comment;

  if (!(format = malloc(lang_width + pattern_width + comment_width + 1))) {
    error(EXIT_FAILURE, "Cannot alloc format string buffer");
  }

  format[lang_width+pattern_width+comment_width] = '\0';
  if (0 > sprintf(format, "%%-%ds%%-%ds%%-%ds\n", lang_width, pattern_width, comment_width)) {
    error(EXIT_FAILURE, "Cannot generat format string");
  }

  printf(format, "LANGUAGE", "PATTERN", "COMMENT");

  list_reset(&lang_pattern_list);
  while ((lang_pattern = (struct lang_match_pattern *) list_current(&lang_pattern_list))) {
    struct sq_list *comment_list;
    struct comment *comment;

    comment_list = (struct sq_list *) hash_table_find(lang_comment_table, lang_pattern->lang);

    list_reset(comment_list);
    while ((comment = (struct comment *) list_current(comment_list))) {
      char buf[MAX_COMMENT_SIZE];
      int len;

      len = comment->start.len;
      strncpy(buf, comment->start.val, len);

      if (comment->end.len) {
        buf[len] = ' ';

        len++;
        strncpy(buf + len, comment->end.val, comment->end.len);
        len += comment->end.len;
      }

      buf[len] = '\0';

      if (comment_list->current == 0) {
        printf(format, lang_pattern->lang, lang_pattern->pattern, buf);
      } else {
        printf(format, "", "", buf);
      }

      list_next(comment_list);
    }

    list_next(&lang_pattern_list);
  }
}

static void create_line_counter(struct bucket *bktp, const char *key) {
  char *lang_key;
  struct line_counter *counter;

  if (!(counter = calloc(1, sizeof(struct line_counter)))) {
    error(EXIT_FAILURE, "Cannot alloc line counter");
  }

  lang_str_cpy(lang_key, key);
  bktp->key = lang_key;
  bktp->value = counter;
}

static void print_result() {
  struct line_counter *file_counter, *lang_counter;
  struct line_counter total_counter;
  struct hash_table *line_counter_table;
  int lang_width, blank_width, code_width, comment_width;
  char *format;

  lang_width = (sizeof("LANGUAGE") > field_width.lang ? sizeof("LANGUAGE") : field_width.lang) + GAP_WIDTH;
  code_width = sizeof("CODE LINES") + GAP_WIDTH;
  comment_width = sizeof("COMMENT LINES") + GAP_WIDTH;
  blank_width = sizeof("BLANK LINES");

  if (!(format = malloc(lang_width + code_width + comment_width + blank_width + 1))) {
    error(EXIT_FAILURE, "Cannot alloc format string buffer");
  }

  format[lang_width+code_width+comment_width+blank_width] = '\0';

  /* header format string */
  if (0 > sprintf(format, "%%-%ds%%-%ds%%-%ds%%-%ds\n", lang_width, code_width, comment_width, blank_width)) {
    error(EXIT_FAILURE, "Cannot generate header format string");
  }

  printf(format, "LANGUAGE", "CODE LINES", "COMMENT LINES", "BLANK LINES");

  /* body format string */
  if (0 > sprintf(format, "%%-%ds%%-%dd%%-%dd%%-%dd\n", lang_width, code_width, comment_width, blank_width)) {
    error(EXIT_FAILURE, "Cannot generate body format string");
  }

  init_hash_table(&line_counter_table, 8);

  list_reset(&line_counter_list);
  while ((file_counter = (struct line_counter *) list_current(&line_counter_list))) {
    lang_counter = (struct line_counter *) hash_table_find_with_add(line_counter_table, file_counter->lang, create_line_counter);
    lang_counter->lang = file_counter->lang;
    lang_counter->blank_lines += file_counter->blank_lines;
    lang_counter->code_lines += file_counter->code_lines;
    lang_counter->comment_lines += file_counter->comment_lines;

    if (verbose) {
      puts(file_counter->filename);
      printf(format, file_counter->lang, file_counter->code_lines, file_counter->comment_lines, file_counter->blank_lines);
    }

    list_next(&line_counter_list);
  }

  if (verbose) {
    puts("");
  }

  total_counter.blank_lines = 0;
  total_counter.code_lines = 0;
  total_counter.comment_lines = 0;
  hash_table_reset(line_counter_table);
  while ((lang_counter = (struct line_counter *) hash_table_current(line_counter_table))) {
    total_counter.blank_lines += lang_counter->blank_lines;
    total_counter.code_lines += lang_counter->code_lines;
    total_counter.comment_lines += lang_counter->comment_lines;

    printf(format, lang_counter->lang, lang_counter->code_lines, lang_counter->comment_lines, lang_counter->blank_lines);

    hash_table_next(line_counter_table);
  }

  printf(format, "", total_counter.code_lines, total_counter.comment_lines, total_counter.blank_lines);
}

static void init_data_struct() {
  init_sq_list(&lang_pattern_list, INIT_PATTERN_LIST_SIZE);
  init_sq_list(&line_counter_list, INIT_LINE_COUNTER_LIST_SIZE);
  init_hash_table(&lang_comment_table, INIT_LANG_COMMENT_TABLE_SIZE);

  if (exclude_list_size) {
    init_sq_list(&exclude_list, exclude_list_size);
  }
}

#define PATTERN_MAX 128
static void add_exclude_list_from_file(const char *exclude_file) {
  FILE *stream;
  char line[PATTERN_MAX];

  if (!(stream = fopen(exclude_file, "r"))) {
    error(EXIT_FAILURE, "Cannot open exclude file: %s", exclude_file);
  }

  while (fgets(line, PATTERN_MAX, stream)) {
    int len = strlen(line);
    char *pos, *buf;

    if (len > PATTERN_MAX - 1) {
      error(EXIT_FAILURE, "Too lang exclude pattern: %s", line);
    }

    if ((pos = strchr(line, '\n'))) {
      len = pos - line;
    }

    if (!(buf = malloc(len + 1))) {
      error(EXIT_FAILURE, "Cannot alloc exclude pattern buffer");
    }

    strncpy(buf, line, len);
    buf[len] = '\0';
    list_append(&exclude_list, buf);
  }
}

static void usage() {
  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file\n");
  puts("Options\n\
    --custom-comment-defs=FILE    define own comment definition\n\
    --comment-defs-detail         show comment definition detail\n\
    --exclude=PATTERN             skip count files matching PATTERN\n\
    --exclude-from=FILE           skip count files matching any pattern from FILE(separate by new line)\n\
    -v, --verbose                 show verbose result\n\
    --version                     version number\n\
    -h, --help                    this help text");
}

enum {
  COMMENT_DEFS_DETAIL_OPTION = CHAR_MAX + 1,
#ifdef DEBUG
  DEBUG_OPTION,
  PARTIAL_TEST_BUF_SIZE_OPTION,
#endif
  EXCLUDE_OPTION,
  EXCLUDE_FROM_OPTION,
  VERSION_OPTION,
};

static const struct option long_opts[] = {
  { "custom-comment-defs", required_argument, NULL, 'c' },
  { "comment-defs-detail", no_argument, NULL, COMMENT_DEFS_DETAIL_OPTION },
#ifdef DEBUG
  { "debug", no_argument, NULL, DEBUG_OPTION },
#endif
  { "exclude", required_argument, NULL, EXCLUDE_OPTION },
  { "exclude-from", required_argument, NULL, EXCLUDE_FROM_OPTION },
  { "verbose", no_argument, NULL, 'v' },
  { "version", no_argument, NULL, VERSION_OPTION },
  { "help", no_argument, NULL, 'h' },
  { NULL, 0, NULL, 0 },
};

int main(int argc, char *argv[]) {
  int opt, i, parse_ret;
  char pathname[PATH_MAX+1];
  boolean has_custom_comment_defs = FALSE;
  char comment_defs_file[PATH_MAX+1];
  char *exclude_pattern = NULL;
  char exclude_file[PATH_MAX+1];
  struct stat sb;

  while ((opt = getopt_long(argc, argv, "vh?", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'c':
      has_custom_comment_defs = TRUE;
      strcpy(comment_defs_file, optarg);
      break;
    case COMMENT_DEFS_DETAIL_OPTION:
      show_comment_defs = TRUE;
      break;
#ifdef DEBUG
    case DEBUG_OPTION:
      debug = TRUE;
      break;
#endif
    case EXCLUDE_OPTION:
      if (!(exclude_pattern = malloc(strlen(optarg) + 1))) {
        error(EXIT_FAILURE, "Cannot alloc exclude pattern");
      }

      strcpy(exclude_pattern, optarg);
      exclude_list_size = exclude_list_size ? exclude_list_size : 1;
      break;
    case EXCLUDE_FROM_OPTION:
      strcpy(exclude_file, optarg);
      exclude_list_size = 16;
      break;
    case 'v':
      verbose = TRUE;
      break;
    case VERSION_OPTION:
      printf("%s\n", HCC_VERSION);
      exit(EXIT_SUCCESS);
    case 'h':
    case '?':
      /* TODO: test option errors */
      usage();
      exit(EXIT_SUCCESS);
    default:
      printf("unknown option: %d", opt);
      usage();
      exit(EXIT_FAILURE);
    }
  }

  init_data_struct();

  /* set custom comment def first */
  if (has_custom_comment_defs) {
    if ((parse_ret = ini_parse(comment_defs_file, build_comment_def, NULL))) {
      error(EXIT_FAILURE, "parse ini file error: %d\nini file: %s\n", parse_ret, comment_defs_file);
    }
  }

  /* default comment defs */
  if ((parse_ret = ini_parse_string(comment_defs_string, build_comment_def, NULL))) {
    error(EXIT_FAILURE, "parse ini string error: %d\nini string:\n%s\n", parse_ret, comment_defs_string);
  }

  if (show_comment_defs) {
    display_comment_defs_detail();
    exit(EXIT_SUCCESS);
  }

  if (exclude_list_size) {
    if (exclude_pattern) {
      list_append(&exclude_list, (void *) exclude_pattern);
    }

    if (exclude_list_size > 1) {
      add_exclude_list_from_file(exclude_file);
    }
  }

  if (!argv[optind]) {
    puts("File or directory argument is required");
    usage();
    exit(EXIT_FAILURE);
  }

  for (i = optind; argv[i]; i++) {
    if (!realpath(argv[i], pathname)) {
      fprintf(stderr, "Error: cannot locat file or directory: %s\n", argv[i]);
      exit(EXIT_FAILURE);
    }

    /* reset stat buffer */
    memset(&sb, 0, sizeof(struct stat));
    stat(pathname, &sb);

    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:
      scan_file(pathname);
      break;
    case S_IFDIR:
#ifdef DEBUG
      printf("scan from dir: %s\n", pathname);
#endif

      if (nftw(pathname, count_for_file, MAX_FTW_FD, 0)) {
        fputs("Fatal: file tree walk failed", stderr);
        exit(EXIT_FAILURE);
      }
      break;
    default:
      fprintf(stderr, "Error: unknown file type: %s\n", pathname);
      break;
    }
  }

  print_result();

  exit(EXIT_SUCCESS);
}
