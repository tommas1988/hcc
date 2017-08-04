#include <stdio.h>
#include <getopt.h>

static void usageInfo(const char *msg) {
  if (msg) {
    puts(msg);
  }

  puts("Usage: hcc [OPTION]... [FILE]...");
  puts("Count the actual code lines in each file");
}

int main(int argc, char *argv[]) {
  const char *shortOpts = "h?";
  const struct option longOpts[] = {
    { "help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0},
  };
  int opt;
  char *filename;

  while ((opt = getopt_long(argc, argv, shortOpts, longOpts, NULL)) != -1) {
    switch (opt) {
    case 'h':
    case '?':
      usageInfo(NULL);
      break;
    default:
      /* never reach to this place */
      break;
    }
  }

  if (!argv[optind]) {
    usageInfo("File or directory argument is required");
  }

  return 0;
}
