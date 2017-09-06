#ifndef __HCC_H
#define __HCC_H

#define MAX_FTW_FD 10

#define MAX_COMMENT_SIZE 50
#define BUFFER_SIZE (16 * 1024)

#define INIT_LANG_TABLE_SIZE 32
#define INIT_COMMENT_TABLE_SIZE 32

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

struct lang_comment_definition {
  char *lang;
  struct comment **comments;
};

struct lang_identity {
  char *pattern;
  char *lang;
};

struct line_counter {
  char *filename;
  char *lang;
  int comment_lines;
  int blank_lines;
  int code_lines;
};

struct line_counter_list_entry {
  struct line_counter counter;
  struct line_counter_list_entry *next;
};

struct line_counter_list {
  struct line_counter_list_entry *head;
  struct line_counter_list_entry *tail;
};

#endif
