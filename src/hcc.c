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
#include "hash.h"
#include "hcc.h"

#include "comment_definitions.c"

static struct hash_table *comment_table;
static struct hash_table *lang_table;

static struct hash_table *lang_comment_list_table;

static struct line_counter_list counter_list = {
  NULL, NULL
};

static struct comment_str c_style_comment = {
  { sizeof("/*") - 1, "/*" },
  { sizeof("*/") - 1, "*/" }
};

static struct comment_str cpp_style_comment = {
  { sizeof("//") - 1, "//" },
  { 0, }
};

static struct comment_str sharp_comment = {
  { sizeof("#") - 1, "#" },
  { 0, }
};

static void strtolower(char *str) {
  while (*str) {
    *str = tolower(*str);
    str++;
  }
}

static struct lang_comment_list **find_comment_list(const char *key, struct hash_table *comment_list_table, int free) {
  unsigned long hash = hash_func(key);
  int i, id;

  for (i = 0; i < comment_list_table->size; i++) {
    id = ((hash) + i * ((hash & 1) ? hash : (hash + 1))) % comment_list_table->size;
    if (free && !comment_list_table->buckets[id]) {
      return &comment_list_table->buckets[id];
    } else if (!free && comment_list_table->buckets[id]) {
      return &comment_list_table->buckets[id];
    }
  }

  return NULL;
}

static struct lang_comment_list *get_lang_comment_list(const char *filename) {
  char buf[FILENAME_MAX];
  char *p, *bname;
  struct lang_comment_list **comments;

  strcpy(buf, filename);
  bname = basename(buf);
  strtolower(bname);

  /* find comments by file extension first */
  if ((p = strrchr(bname, '.'))) {
    comments = find_comment_list(p, lang_comment_list_table, 0);

    if (comments) {
      return *comments;
    }
  } else {
    comments = NULL;
  }

  if (!comments) {              /* find comments by filename */
    comments = find_comment_list(bname, lang_comment_list_table, 0);
  }

  if (comments) {
    return *comments;
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

static void build_lang_comment_list_table() {
  unsigned int lang_size = 5;

  unsigned int table_size = 1;
  struct lang_comment_list **slot;

  /* hash_m must be power of 2 */
  while (lang_size >>= 1) {
    table_size <<= 1;
  }
  table_size <<= 1;

  lang_comment_list_table = calloc(1, sizeof(struct hash_table) + sizeof(struct lang_comment_list *) * table_size);
  lang_comment_list_table->size = table_size;

  struct lang_comment_list *c_comment = malloc(sizeof(struct lang_comment_list) + sizeof(struct comment_str *) * 3);
  c_comment->lang = "c";
  c_comment->list[0] = &c_style_comment;
  c_comment->list[1] = &cpp_style_comment;
  c_comment->list[2] = NULL;

  struct lang_comment_list *cpp_comment = malloc(sizeof(struct lang_comment_list) + sizeof(struct comment_str *) * 3);
  cpp_comment->lang = "c++";
  cpp_comment->list[0] = &c_style_comment;
  cpp_comment->list[1] = &cpp_style_comment;
  cpp_comment->list[2] = NULL;

  struct lang_comment_list *shell_comment = malloc(sizeof(struct lang_comment_list) + sizeof(struct comment_str *) * 2);
  shell_comment->lang = "shell";
  shell_comment->list[0] = &sharp_comment;
  shell_comment->list[1] = NULL;

  struct lang_comment_list *php_comment = malloc(sizeof(struct lang_comment_list) + sizeof(struct comment_str *) * 4);
  php_comment->lang = "php";
  php_comment->list[0] = &c_style_comment;
  php_comment->list[1] = &cpp_style_comment;
  php_comment->list[2] = &sharp_comment;
  php_comment->list[3] = NULL;

  /* .h .c file extension for c comment */
  slot = find_comment_list(".h", lang_comment_list_table, 1);
  *slot = c_comment;
  slot = find_comment_list(".c", lang_comment_list_table, 1);
  *slot = c_comment;

  /* .cpp file extension for c++ comment */
  slot = find_comment_list(".cpp", lang_comment_list_table, 1);
  *slot = cpp_comment;

  /* .sh file extension for shell comment */
  slot = find_comment_list(".sh", lang_comment_list_table, 1);
  *slot = shell_comment;

  /* .php file extension for php comment */
  slot = find_comment_list(".php", lang_comment_list_table, 1);
  *slot = php_comment;
}

static struct lang_comment_list **find_comment_list(const char *key, struct hash_table *comment_list_table, int free) {
  unsigned long hash = hash_func(key);
  int i, id;

  for (i = 0; i < comment_list_table->size; i++) {
    id = ((hash) + i * ((hash & 1) ? hash : (hash + 1))) % comment_list_table->size;
    if (free && !comment_list_table->buckets[id]) {
      return &comment_list_table->buckets[id];
    } else if (!free && comment_list_table->buckets[id]) {
      return &comment_list_table->buckets[id];
    }
  }

  return NULL;
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

static struct comment *get_comment(const char *str) {
}

static struct lang_comment_definition *get_comment_definition(const char *lang) {
}

static int build_comments_table(void* user, const char* lang, const char* name, const char* value) {
  printf("language: %s\n name: %s\nvalue: %s\n", lang, name, value);
  return 0;
}

int main(int argc, char *argv[]) {
  const char *short_opts = "h?";
  const struct option long_opts[] = {
    { "help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0},
  };
  int opt, i;
  char pathname[PATH_MAX+1];
  struct stat sb;

  while ((opt = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
    switch (opt) {
    case 'h':
    case '?':
      usage();
      break;
    default:
      /* never reach to this place */
      break;
    }
  }

  init_hash_table(comment_table, INIT_COMMENT_TABLE_SIZE);
  init_hash_table(lang_table, INIT_LANG_TABLE_SIZE);

  ini_parse_string(comment_definitions, build_comments_table, NULL);
  exit(EXIT_SUCCESS);

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
