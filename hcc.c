#define _XOPEN_SOURCE 500       /* required by nftw */
#define _POSIX_C_SOURCE 200112L /* required by posix_fadvise */

#include <stdarg.h>
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

#include "hcc.h"

static struct hash_table *lang_comment_table;

static struct line_counter_list counter_list = {
  NULL, NULL
};

static struct comment c_style_comment = {
  { sizeof("/*") - 1, "/*" },
  { sizeof("*/") - 1, "*/" }
};

static struct comment cpp_style_comment = {
  { sizeof("//") - 1, "//" },
  { 0, }
};

static struct comment sharp_comment = {
  { sizeof("#") - 1, "#" },
  { 0, }
};

static void error(const char *format, ...) {
  va_list var_arg;
  char buf[ERR_BUF_SIZE];

  va_start(var_arg, format);
  vsnprintf(buf, ERR_BUF_SIZE, format, var_arg);
  va_end(var_arg);

  perror(buf);
}

static void strtolower(char *str) {
  while (*str) {
    *str = tolower(*str);
    str++;
  }
}

static unsigned long hash_func(const char *key) {
  const char *p;
  unsigned long hash = 0;

  for (p = key; *p != '\0'; p++) {
    hash = hash * 33 + *p;
  }

  return hash;
}

static struct lang_comment **find_comment(const char *key, struct hash_table *lang_comment_table, int free) {
  unsigned long hash = hash_func(key);
  int i, id;

  for (i = 0; i < lang_comment_table->size; i++) {
    id = ((hash) + i * ((hash & 1) ? hash : (hash + 1))) % lang_comment_table->size;
    if (free && !lang_comment_table->buckets[id]) {
      return &lang_comment_table->buckets[id];
    } else if (!free && lang_comment_table->buckets[id]) {
      return &lang_comment_table->buckets[id];
    }
  }

  return NULL;
}

static struct lang_comment *get_lang_comment(const char *filename) {
  char buf[FILENAME_MAX];
  char *p, *bname;
  struct lang_comment **comments;

  strcpy(buf, filename);
  bname = basename(buf);
  strtolower(bname);

  /* find comments by file extension first */
  if ((p = strrchr(bname, '.'))) {
    comments = find_comment(p, lang_comment_table, 0);

    if (comments) {
      return *comments;
    }
  } else {
    comments = NULL;
  }

  if (!comments) {              /* find comments by filename */
    comments = find_comment(bname, lang_comment_table, 0);
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
      counter->comment_line++;                  \
      break;                                    \
    case COUNTER_BLANK:                         \
      counter->blank_line++;                    \
      break;                                    \
    case COUNTER_CODE:                          \
      counter->code_line++;                     \
      break;                                    \
    }                                           \
  } while (0)                                   \


static void count_line(const char *filename) {
  int fd = open(filename, O_RDONLY);
  char wh_buf[MAX_COMMENT_SIZE + BUFFER_SIZE], *buf;
  ssize_t bytes_read, pos;
  int in_testing = 1, in_comment = 0, met_end_comment = 0;
  int filename_len;
  struct lang_comment *comments;
  struct comment *cp;
  struct line_counter *counter;

  if (!(counter = malloc(sizeof(struct line_counter)))) {
    error("Cannot alloc counter");
    exit(EXIT_FAILURE);
  }

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
  counter->next = NULL;
  if (!counter_list.head) {
    counter_list.head = counter;
    counter_list.tail = counter;
  } else {
    counter_list.tail->next = counter;
    counter_list.tail = counter;
  }

  if (!(comments = get_lang_comment(filename))) {
    /* count next file */
    return;
  }

  if (fd == -1) {
    error("Cannot open file: %s", filename);
    exit(EXIT_FAILURE);
  }

  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  buf = &wh_buf[MAX_COMMENT_SIZE];
  while ((bytes_read = read(fd, buf, BUFFER_SIZE))) {
    if (bytes_read == -1) {
      error("Read file %s error", filename);
      exit(EXIT_FAILURE);
    }

    pos = pos < 0 ? pos : 0;
    while (pos >= 0 && pos < bytes_read) {
      if (in_testing) { /* test blank line and the beginning of comments */
        switch (buf[pos]) {
        case ' ':
        case '\t':
        case '\v':
        case '\r':
          pos++;
          break;
        case '\n':
          pos++;
          update_counter(COUNTER_BLANK, counter);
          break;
        default:
          in_testing = 0;
          break;
        }

        if (!in_testing) {      /* test match comment */
          int i = 0;
          int bytes_left = bytes_read - pos, bytes_match = 0;
          cp = comments->list[i];
          while (cp) {
            int len = bytes_left < cp->begin.len ? bytes_left : cp->begin.len;
            if (!strncmp(cp->begin.val, buf + pos, len)) {
              bytes_match = len;
              if (bytes_left < cp->begin.len) { /* partial match */
                strncpy(buf - len, buf + pos, len);
                pos = -len;
                in_testing = 1;
              } else {
                in_comment = 1;
              }
              break;
            }
            cp = comments->list[++i];
          }

          if (pos < 0) {        /* partial match, refill buffer */
            break;
          }

          pos += (bytes_match ? bytes_match : 1);
        }
      } else if (in_comment) {  /* test the end of comments */
        if (cp->end.len && cp->end.val[0] == buf[pos]) {
          int bytes_left = bytes_read - pos;
          int len = bytes_left < cp->end.len ? bytes_left : cp->end.len;

          if (!strncmp(cp->end.val, buf + pos, len)) {
            if (bytes_left < cp->end.len) { /* partial match */
              strncpy(buf - len, buf + pos, len);
              pos = -len;
              break;            /* refill buffer */
            }

            pos += len;
            met_end_comment = 1;
            continue;
          }
        } else if (buf[pos] == '\n') {
          update_counter(COUNTER_COMMENT, counter);
          if (met_end_comment) {
            in_comment = met_end_comment = 0;
          }
          if (!cp->end.len) {   /* inline comment */
            in_comment = 0;
          }
        }

        pos++;
      } else {                  /* test new line character */
        if (buf[pos] == '\n') {
          update_counter(COUNTER_CODE, counter);
          in_testing = 1;
        }
        pos++;
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

static void build_lang_comment_hash_table() {
  unsigned int lang_size = 5;

  unsigned int table_size = 1;
  struct lang_comment **slot;

  /* hash_m must be power of 2 */
  while (lang_size >>= 1) {
    table_size <<= 1;
  }
  table_size <<= 1;

  lang_comment_table = calloc(1, sizeof(struct hash_table) + sizeof(struct lang_comment *) * table_size);
  lang_comment_table->size = table_size;

  struct lang_comment *c_comment = malloc(sizeof(struct lang_comment) + sizeof(struct comment *) * 3);
  c_comment->lang = "c";
  c_comment->list[0] = &c_style_comment;
  c_comment->list[1] = &cpp_style_comment;
  c_comment->list[2] = NULL;

  struct lang_comment *cpp_comment = malloc(sizeof(struct lang_comment) + sizeof(struct comment *) * 3);
  cpp_comment->lang = "c++";
  cpp_comment->list[0] = &c_style_comment;
  cpp_comment->list[1] = &cpp_style_comment;
  cpp_comment->list[2] = NULL;

  struct lang_comment *shell_comment = malloc(sizeof(struct lang_comment) + sizeof(struct comment *) * 2);
  shell_comment->lang = "shell";
  shell_comment->list[0] = &sharp_comment;
  shell_comment->list[1] = NULL;

  struct lang_comment *php_comment = malloc(sizeof(struct lang_comment) + sizeof(struct comment *) * 4);
  php_comment->lang = "php";
  php_comment->list[0] = &c_style_comment;
  php_comment->list[1] = &cpp_style_comment;
  php_comment->list[2] = &sharp_comment;
  php_comment->list[3] = NULL;

  /* .h .c file extension for c comment */
  slot = find_comment(".h", lang_comment_table, 1);
  *slot = c_comment;
  slot = find_comment(".c", lang_comment_table, 1);
  *slot = c_comment;

  /* .cpp file extension for c++ comment */
  slot = find_comment(".cpp", lang_comment_table, 1);
  *slot = cpp_comment;

  /* .sh file extension for shell comment */
  slot = find_comment(".sh", lang_comment_table, 1);
  *slot = shell_comment;

  /* .php file extension for php comment */
  slot = find_comment(".php", lang_comment_table, 1);
  *slot = php_comment;
}

static void print_result() {
  struct line_counter *counter = counter_list.head;

  while (counter) {
    puts(counter->filename);
    printf("\tCommnet line: %d\n\tBlank line: %d\n\tCode line: %d\n", counter->comment_line, counter->blank_line, counter->code_line);
    counter = counter->next;
  }
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

  if (!argv[optind]) {
    fputs("File or directory argument is required", stderr);
    usage();
    exit(EXIT_FAILURE);
  }

  build_lang_comment_hash_table();

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
