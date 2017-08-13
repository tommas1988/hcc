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

#include <ftw.h>

#include "../deps/inih/ini.h"

#include "error.h"
#include "sq_list.h"
#include "hash.h"
#include "hcc.h"

#include "comment_def_config.c"

static struct sq_list lang_pattern_list;
static struct hash_table *lang_comment_table;

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
  } while (0)                                   \


static void count_line(const char *filename) {
  int fd;
  char buf[MAX_COMMENT_SIZE + BUFFER_SIZE], *read_buf;
  ssize_t bytes_read, pos;
  boolean in_code = FALSE, in_comment = FALSE, match_end_comment = FALSE;
  int filename_len;
  struct lang_comment_list *comments;
  struct comment_str *csp;
  /* TODO: rename list_entry */
  struct line_counter_list_entry *list_entry;
  struct line_counter *counter;

  if (!(comments = get_lang_comment_list(filename))) {
    /* count next file */
    return;
  }

  if (!(list_entry = malloc(sizeof(struct line_counter_list_entry)))) {
    error("Cannot alloc counter");
    exit(EXIT_FAILURE);
  }
  list_entry->next = NULL;
  counter = &list_entry->counter;

  filename_len = strlen(filename);
  if (filename_len > FILENAME_MAX) {
    error("Too long file name");
    exit(EXIT_FAILURE);
  }

  if (!(counter->filename = malloc(filename_len))) {
    error("Cannot alloc filename");
    exit(EXIT_FAILURE);
  }

  strncpy(counter->filename, filename, filename_len);
  counter->lang = comments->lang;
  if (!counter_list.head) {
    counter_list.head = list_entry;
    counter_list.tail = list_entry;
  } else {
    counter_list.tail->next = list_entry;
    counter_list.tail = list_entry;
  }

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    error("Cannot open file: %s", filename);
    exit(EXIT_FAILURE);
  }

  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  read_buf = &buf[MAX_COMMENT_SIZE];
  while ((bytes_read = read(fd, read_buf, BUFFER_SIZE))) {
    if (bytes_read == -1) {
      error("Read file %s error", filename);
      exit(EXIT_FAILURE);
    }

    pos = pos < 0 ? pos : 0;
    while (pos >= 0 && pos < bytes_read) {
      if (in_code) {
        if (read_buf[pos] == '\n') {
          update_counter(COUNTER_CODE, counter);
          /* set in_code to FALSE in order to test next line type */
          in_code = FALSE;
        }
        pos++;
      } else if (in_comment) {
        if (csp->end.len && csp->end.val[0] == read_buf[pos]) {
          int bytes_left = bytes_read - pos;
          int len = bytes_left < csp->end.len ? bytes_left : csp->end.len;

          if (!strncmp(csp->end.val, read_buf + pos, len)) {
            if (bytes_left < csp->end.len) { /* partial match */
              strncpy(read_buf - len, read_buf + pos, len);
              pos = -len;
              break;            /* refill buffer */
            }

            pos += len;
            match_end_comment = TRUE;
          } else {
            pos++;
          }
        } else if (read_buf[pos] == '\n') {
          update_counter(COUNTER_COMMENT, counter);

          if (match_end_comment) {
            in_comment = match_end_comment = FALSE;
          } else if (!csp->end.len) {   /* inline comment */
            in_comment = FALSE;
          }

          pos++;
        } else if (isspace(read_buf[pos])) { /* not a space charactor */
          in_code = TRUE;
          in_comment = match_end_comment = FALSE;
          pos++;
        }
      } else {
        if (read_buf[pos] == '\n') {
          update_counter(COUNTER_BLANK, counter);
          pos++;
        } else if (isspace(read_buf[pos])) { /* not space charactor */
          int bytes_left = bytes_read - pos, bytes_match = 0;
          int i = 0;

          csp = comments->list[i];
          while (csp) {
            int len = bytes_left < csp->start.len ? bytes_left : csp->start.len;
            if (!strncmp(csp->start.val, read_buf + pos, len)) {
              bytes_match = len;
              if (bytes_left < csp->start.len) { /* partial match */
                strncpy(read_buf - len, read_buf + pos, len);
                pos = -len;
              } else {
                in_comment = TRUE;
              }
              break;
            }
            csp = comments->list[++i];
          }

          if (pos < 0) {        /* partial match, refill buffer */
            break;
          }

          pos += (bytes_match ? bytes_match : 1);
        }
      }
    }
  }
}

static int count_for_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  if (typeflag == FTW_F) {
    count_line(fpath);
  }
  return 0;
}

static void usage() {
  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file");
}

static void print_result() {
  struct line_counter_list_entry *list_entry = counter_list.head;
  struct line_counter *counter;

  while (list_entry) {
    counter = &(list_entry->counter);
    puts(counter->filename);
    puts(counter->lang);
    printf("\tCommnet line: %d\n\tBlank line: %d\n\tCode line: %d\n", counter->comment_lines, counter->blank_lines, counter->code_lines);
    list_entry = list_entry->next;
  }
}

#define lang_str_cpy (dest, lang_str)                   \
  do {                                                  \
    (dest) = malloc(MAX_LANG_SIZE + 1);                 \
    if (!(dest)) {                                      \
      error(EXIT_FAILURE, "Cannot alloc lang key");     \
    }                                                   \
                                                        \
    strncpy((dest), (lang_str), MAX_LANG_SIZE + 1);     \
  } while (0)

static void create_comment_list(bucket *bktp, const char *key) {
  char *lang_key;
  struct sq_list *comment_list;

  comment_list = malloc(sizeof(struct sq_list));
  init_sq_list(comment_list, INIT_LANG_COMMENT_LIST_SIZE);

  lang_str_cpy(lang_key, key);
  bktp->key = lang_key;
  bktp->value = comment_list;
}

static comment *create_comment_from_string(const char *str) {
  struct comment *comment;
  char *p;

  comment = malloc(sizeof(struct comment));
  if (!comment) {
    error(EXIT_FAILURE, "Cannot alloc comment");
  }

  /* TODO: trim leading & trailing space charactors first */

  p = strchr(str, ' ');
  if (p) {
    int start_len = p - str;

    comment->start.len = start_len;
    comment->start.val = str;
    comment->end.len = strlen(str) - start_len - 1;
    comment->end.val = p + 1;
  } else {
    comment->start.len = strlen(str);
    comment->start.val = str;
    comment->end.len = 0;
    comment->end.val = NULL;
  }

  return comment;
}

#define strtolower (str)                        \
  do {                                          \
    while ((*str)) {                            \
      (*str) = tolower((*str));                 \
      str++;                                    \
    }                                           \
  } while (0)

static int build_comment_def(void* unused, const char* lang, const char* name, const char* value) {
  char clang[MAX_LANG_SIZE + 1];

  strncpy(clang, lang, MAX_LANG_SIZE);
  clang[MAX_LANG_SIZE] = '\0';

  strtolower(clang);

  if (!strcmp(name, "pattern")) {
    char *lang_key;

    lang_str_cpy(lang_key, clang);
    list_append(&lang_pattern_list, lang_key);
  } else if (!strcmp(name, "comment")) {
    struct sq_list *comment_list;

    comment_list = (struct sq_list *) hash_table_find_with_add(lang_comment_table, clang, create_comment_list);
    list_append(comment_list, (void *) create_comment_from_string(value));
  } else {
    error(EXIT_FAILURE, "Unknown comment definition field name");
  }

  return 0;
}

static void init_data_struct() {
  init_sq_list(&lang_pattern_list, INIT_PATTERN_LIST_SIZE);
  init_hash_table(lang_comment_table, INIT_LANG_COMMENT_TABLE_SIZE);
}

static void display_lang_comment_match_pattern() {
  struct lang_match_pattern *lang_pattern;

  list_reset(&lang_pattern_list);
  lang_pattern = (lang_match_pattern *) list_next(&lang_pattern_list);

  puts("LANGUAGE\tPATTERN\tCOMMENT");

  while (pattern) {
    struct sq_list *comment_list;
    struct comment *comment;

    printf("%s\t%s", lang_pattern->lang, lang_pattern->pattern);

    comment_list = (struct sq_list *) hash_table_find(lang_comment_table, lang_pattern->lang);
    comment = (struct comment *) list_next(comment_list);
    while (comment) {
      char buf[MAX_COMMENT_SIZE];
      int len;

      len = comment->start.len;
      strncpy(buf, comment->start.val, len);

      if (comment->end.len) {
        len++;
        strncpy(buf + len, comment->end.val, comment->end.len);
        len += comment->end.len;
      }

      buf[len] = '\0';

      if (comment_list->current == 0) {
        printf("\t%s", buf);
      } else {
        printf(", %s", buf);
      }
    }
    putchar('\n');
  }
}

int main(int argc, char *argv[]) {
  const char *short_opts = "h?";
  const struct option long_opts[] = {
    { "match-pattern", no_argument, NULL, 'P' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 },
  };
  int opt, i;
  char pathname[PATH_MAX + 1];
  boolean show_match_pattern = FALSE;
  struct stat sb;

  while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
    switch (opt) {
    case 'P':
      show_match_pattern = TRUE;
      break;
    case 'h':
    case '?':
      usage();
      break;
    default:
      /* never reach to this place */
      break;
    }
  }

  ini_parse_string(comment_def_confg, build_comment_def, NULL);

  if (show_match_pattern) {
    display_lang_comment_match_pattern();
    exit(EXIT_SUCCESS);
  }

  if (!argv[optind]) {
    fputs("File or directory argument is required", stderr);
    usage();
    exit(EXIT_FAILURE);
  }

  build_lang_comment_list_table();

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
      count_line(pathname);
      break;
    case S_IFDIR:
      printf("scan from dir: %s\n", pathname);
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
