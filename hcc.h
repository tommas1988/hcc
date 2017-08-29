#ifndef __HCC_H
#define __HCC_H

#include <unistd.h>

#define MAX_FTW_FD 10

#define MAX_COMMENT_SIZE 50
#define ERR_BUF_SIZE 100
#define BUFFER_SIZE (16 * 1024)

struct comment_str {
  int len;
  char *val;
};

struct comment {
  struct comment_str begin;
  struct comment_str end;
};

struct lang_comment {
  char *lang;
  struct comment **list;
};

struct hash_table {
  unsigned int size;
  struct lang_comment *buckets[];
};

struct line_counter {
  char *filename;
  int comment_line;
  int blank_line;
  int code_line;
  struct line_counter *next;
};

struct line_counter_list {
  struct line_counter *head;
  struct line_counter *tail;
};

#endif
