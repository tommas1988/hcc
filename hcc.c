#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define _XOPEN_SOURCE 500       /* required to use nftw */
#include <ftw.h>

#include "hcc.h"

static void scanFile(const char *filename) {
  printf("scan file: %s\n", filename);
}

static int processFile(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
  if (typeflag == FTW_F) {
    scanFile(fpath);
  }
  return 0;
}

static void scanDir(const char *dirname) {
  printf("scan from dir: %s\n", dirname);
  if (nftw(dirname, processFile, MAX_FTW_FD, 0)) {
    fputs("Fatal: file tree walk failed", stderr);
    exit(EXIT_FAILURE);
  }
}

static void usageInfo() {
  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file");
}

int main(int argc, char *argv[]) {
  const char *shortOpts = "h?";
  const struct option longOpts[] = {
    { "help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0},
  };
  int opt, i;
  char path[PATH_MAX+1];
  struct stat sb;

  while ((opt = getopt_long(argc, argv, shortOpts, longOpts, NULL)) != -1) {
    switch (opt) {
    case 'h':
    case '?':
      usageInfo();
      break;
    default:
      /* never reach to this place */
      break;
    }
  }

  if (!argv[optind]) {
    fputs("File or directory argument is required", stderr);
    usageInfo();
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
      scanFile(path);
      break;
    case S_IFDIR:
      scanDir(path);
      break;
    default:
      fprintf(stderr, "Error: unknown file type: %s\n", path);
      break;
    }
  }

  exit(EXIT_SUCCESS);
}
