#ifndef __HCC_H
#define __HCC_H

#include <unistd.h>
#include <limits.h>

#define MAX_FTW_FD 10

#define FILENAME_MAX (NAME_MAX + 1)
#define MAX_EXT_SIZE 20
#define ERR_BUF_SIZE 100
#define BUFFER_SIZE (16 * 1024)

struct hash_table {
  unsigned int size;
  lang_comment *buckets[];
};

struct comment_str {
  int len;
  char *val;
};

struct comment {
  comment_str begin;
  comment_str end;
};

struct lang_comment {
  char *lang;
  struct comment **list;
};

#endif
