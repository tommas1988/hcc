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

#define _XOPEN_SOURCE 500       /* required to use nftw */
#include <ftw.h>

#include "hcc.h"

static struct hash_table *lang_comment_table;

static struct comment c_style_comment = {
  { sizeof("/*"), "/*" },
  { sizeof("*/"), "*/" }
};

static struct comment cpp_style_comment = {
  { sizeof("//"), "//" },
  NULL
};

static struct comment sharp_comment = {
  { sizeof("#"), "#" },
  NULL
};

static struct lang_comment c_comment = {
  "c",
  {
    &c_style_comment,
    $cpp_style_comment,
    NULL
  }
};

static struct lang_comment cpp_comment = {
  "c++",
  {
    &c_style_comment,
    $cpp_style_comment,
    NULL
  }
};

static struct lang_comment shell_comment = {
  "shell",
  {
    &sharp_comment,
    NULL
  }
};

static struct lang_comment php_comment = {
  "php",
  {
    &c_style_comment,
    &cpp_style_comment,
    &sharp_comment,
    NULL
  }
};

static void error(const char *format, ...) {
  va_list var_arg;
  char buf[ERR_BUF_SIZE];

  va_start(var_arg, format);
  vsnprintf(buf, ERR_BUF_SIZE, formar, var_arg);
  va_end(var_arg);

  perror(msg);
}

#define update_counter (is_code)

static void strtolower(char *str) {
  while (*str) {
    *str = tolower(*str++);
  }
}

static struct lang_comment *get_lang_comment(const char *filename) {
  char buf[FILENAME_MAX];
  char *p;
  struct lang_comment **comments;

  strtolower(basename(strcpy(buf, filename)));

  /* find comments by file extension first */
  p = strrchr(buf, '.');
  comments = find_comment(p, lang_comment_table, 0);

  if (*comments) {
    return *comments;
  }

  if (!*comments) {              /* find comments by filename */
    comments = find_comments(buf, lang_comment_table, 0);
  }

  if (*comments) {
    return *comments;
  }

  return NULL;
}

static void count_code_line(const char *filename) {
  int fd = open(filename, O_RDONLY);
  char buf[BUFFER_SIZE], partial_match_comment[MAX_CMNT_SIZE];
  ssize_t bytes_read, pos;
  int in_testing = 1, in_comment = 0, partial_match = 0;
  struct lang_comment *comments = get_lang_comment(filename);
  struct comment *cp;

  if (!comment) {
    /* count next file */
    return;
  }

  if (fd == -1) {
    error("Cannot open file: %s", filename);
    exit(EXIT_FAILURE);
  }

  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

  while ((bytes_read = read(fd, buf, BUFFER_SIZE))) {
    if (bytes_read == -1) {
      error("Read file %s error", filename);
      exit(EXIT_FAILURE);
    }

    pos = 0;
    while (pos < bytes_read) {
      if (in_testing) { /* test blank line and the beginning of comments */
        switch (buf[pos]) {
        case ' ':
        case '\t':
        case '\v':
        case '\r':
          break;
        case '\n':
          update_counter(0);
          break;
        default:
          in_testing = 0;
          break;
        }

        if (!in_testing) {
          cp = comments->list;
          while (cp) {
            
          }
        }

        pos++;
      } else if (in_comment) {  /* test the end of comments */

      } else {                  /* test new line character */

      }
    }
  }
}

static void scan_file(const char *filename) {
  printf("scan file: %s\n", filename);
}

static int process_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  if (typeflag == FTW_F) {
    scan_file(fpath);
  }
  return 0;
}

static void scan_dir(const char *dirname) {
  printf("scan from dir: %s\n", dirname);
  if (nftw(dirname, processFile, MAX_FTW_FD, 0)) {
    fputs("Fatal: file tree walk failed", stderr);
    exit(EXIT_FAILURE);
  }
}

static void usage() {
  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file");
}

static unsigned long hash_func(const char *key) {
  char *p;
  unsigned long hash = 0;

  for (p = key; p != '/0'; p++) {
    hash = hash * 33 + *p;
  }

  return hash;
}

static struct lang_comment **find_comment(const char *key, struct hash_table *lang_comment_table, int free) {
  unsigned long hash = hash_func(key);
  int i, id;

  for (i = 0; i < lang_comment_table->size; i++) {
    id = ((hash) + i * ((hash & 1) ? hash : (hash + 1))) % lang_comment_table->size;
    if (free && !lang_comment_table[id]) {
      return &lang_comment_table[id];
    } else if (!free && lang_comment_table[id]) {
      return &lang_comment_table[id];
    }
  }

  return NULL;
}

static void build_lang_comment_hash_table() {
  unsigned int lang_size = 5;

  unsigned int table_size = 1;
  struct lang_comment **slot;

  /* hash_m must be power of 2 */
  while (lang_size >> 1) {
    table_size << 1;
  }

  lang_comment_table = calloc(1, sizeof(hash_table) + sizeof(lang_comment *) * table_size);
  lang_comment_table->size = table_size;

  /* .h .c file extension for c comment */
  slot = find_comment(".h", lang_comment_table, 1);
  *slot = &c_comment;
  slot = find_comment(".c", lang_comment_table, 1);
  *slot = &c_comment;

  /* .cpp file extension for c++ comment */
  slot = find_comment(".cpp", lang_comment_table, 1);
  *slot = &cpp_comment;

  /* .sh file extension for shell comment */
  slot = find_comment(".sh", lang_comment_table, 1);
  *slot = &sh_comment;

  /* .php file extension for php comment */
  slot = find_comment(".php", lang_comment_table, 1);
  *slot = &php_comment;
}

int main(int argc, char *argv[]) {
  const char *short_opts = "h?";
  const struct option long_opts[] = {
    { "help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0},
  };
  int opt, i;
  char path[PATH_MAX+1];
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

  for (i = optind; argv[i]; i++) {
    if (!realpath(argv[i], path)) {
      fprintf(stderr, "Error: cannot locat file or directory: %s\n", argv[i]);
      exit(EXIT_FAILURE);
    }

    /* reset stat buffer */
    memset(&sb, 0, sizeof(struct stat));
    stat(path, &sb);

    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:
      scan_file(path);
      break;
    case S_IFDIR:
      scan_dir(path);
      break;
    default:
      fprintf(stderr, "Error: unknown file type: %s\n", path);
      break;
    }
  }

  exit(EXIT_SUCCESS);
}
