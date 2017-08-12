#ifndef __HCC_H
#define __HCC_H

#define MAX_FTW_FD 10

#define MAX_COMMENT_SIZE 50
#define ERR_BUF_SIZE 100
#define BUFFER_SIZE (16 * 1024)

#define TRUE 1
#define FALSE 0

typedef int boolean;

struct comment_part_str {
  int len;
  char *val;
};

struct comment_str {
  struct comment_part_str start;
  struct comment_part_str end;
};

struct lang_comment_list {
  char *lang;
  struct comment_str *list[];
};

struct hash_table {
  unsigned int size;
  struct lang_comment_list *buckets[];
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
  struct line_counter *next;
};

struct line_counter_list {
  struct line_counter_list_entry *head;
  struct line_counter_list_entry *tail;
};

#endif
