#include <stdio.h>
#include <getopt.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

static void usageInfo() {
  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file");
}

static void scanFile() {
  puts("scan a code file");
}

static void scanDir() {
  puts("scan from directory");
}

int main(int argc, char *argv[]) {
  const char *shortOpts = "h?";
  const struct option longOpts[] = {
    { "help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0},
  };
  int opt, i;
  char *filename;
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
    fputs(stderr, "File or directory argument is required");
    usageInfo();
    exit(EXIT_FAILURE);
  }

  for (i = optind; argv[i]; i++) {
    /* reset stat buffer */
    memset(&sb, 0, sizeof(struct stat));
    stat(argv[i], &sb);

    switch (sb.st_mode & S_IFMT) {
    case S_IFREG:
      scanFile();
      break;
    case S_IFDIR:
      scanDir();
      break;
    default:
      fprintf(stderr, "Error: cannot locat file or directory: %s\n", argv[i]);
      break;
    }
  }

  exit(EXIT_SUCCESS);
}
