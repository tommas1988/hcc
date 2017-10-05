#ifndef __HCC_H
#define __HCC_H

#include "sq_list.h"

#define MAX_FTW_FD 10

#define MAX_LANG_SIZE 10
#define MAX_COMMENT_SIZE 20
#define BUFFER_SIZE (16 * 1024)

#define INIT_PATTERN_LIST_SIZE 32
#define INIT_LINE_COUNTER_LIST_SIZE 32
#define INIT_LANG_COMMENT_TABLE_SIZE 32
#define INIT_LANG_COMMENT_LIST_SIZE 8

#define GAP_WIDTH 4

#define TRUE 1
#define FALSE 0

typedef int boolean;

struct comment_str {
  int len;
  char *val;
};

struct comment {
  struct comment_str start;
  struct comment_str end;
};

struct lang_match_pattern {
  char *pattern;
  char *lang;
};

struct line_counter {
  char *filename;
  char *lang;
  int comment_lines;
  int blank_lines;
  int code_lines;
#ifdef DEBUG
  long int line_type_info;
#endif
};

#endif
