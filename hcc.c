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

#define _XOPEN_SOURCE 500       /* required to use nftw */
#include <ftw.h>

#include "hcc.h"

static struct comment c_comment = {
  { sizeof("/*"), "/*" },
  { sizeof("*/"), "*/" }
};

static struct comment cpp_comment = {
  { sizeof("//"), "//" },
  NULL
};

static struct comment sharp_comment = {
  { sizeof("#"), "#" },
  NULL
};

/* TODO: need a hash map to find comments */
static lang_comment lang_comment_list[] = {
  {
    "c",
    {
      &c_comment,
      $cpp_comment,
      NULL
    }
  },

  {
    "php",
    {
      &c_comment,
      &cpp_comment,
      &sharp_comment,
      NULL
    }
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

static int get_file_extension(const char *filename, char *buf) {
  size_t len = strlen(filename);
  char *p = (char *) memrchr(filename, '.', len);

  if (!p) {
    return -1;
  }

  /* pass . character */
  p++;

  while (*p) {
    *buf = tolower(*p);
    p++;
    buf++;
  }

  return 0;
}

static struct lang_comment *get_comment_list(const char *filename) {
  char ext_buf[MAX_EXT_SIZE];
  int i;

  if (get_file_extension(filename, ext_buf) == -1) {
    return NULL;
  }

  for (i = 0; i < sizeof(lang_comment_list); i++) {
    
  }

  return NULL;
}

static void count_code_line(const char *filename) {
  int fd = open(filename, O_RDONLY);
  char buf[BUFFER_SIZE], left_partial_comment[MAX_CMNT_SIZE];
  ssize_t bytes_read, pos;
  int in_testing = 1, in_comment = 0;

  struct lang_comment *cp = get_comment_list(filename);
  if (!cp) {
    /* count next file */
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
