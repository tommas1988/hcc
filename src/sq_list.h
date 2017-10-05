#ifndef __HCC_SQ_LIST_H
#define __HCC_SQ_LIST_H

#include <assert.h>
#include <stdlib.h>
#include "error.h"

struct sq_list {
  void **data;
  int length;
  int next_free;
  int current;
};

void init_sq_list(struct sq_list *list, int size);

#define list_reset(list)                        \
  do {                                          \
    (list)->current = 0;                        \
  } while (0)

#define list_next(list)                         \
  do {                                          \
    (list)->current++;                          \
  } while (0)

void *list_current(struct sq_list *list);
void list_append(struct sq_list *list, void *value);

#endif
