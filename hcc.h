#ifndef __HCC_H
#define __HCC_H

#include <unistd.h>

#define MAX_FTW_FD 10

#define MAX_CMNT_SIZE 20
#define MAX_EXT_SIZE 20
#define ERR_BUF_SIZE 100
#define BUFFER_SIZE (16 * 1024)

struct comment_str {
  ssize_t len;
  char *str;
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
