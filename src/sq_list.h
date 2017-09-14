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

#define init_sq_list (list, size)                               \
  do {                                                          \
  assert(((list)) && ((size) > 0));                             \
                                                                \
  (list)->data = malloc(sizeof(void *) * (size));               \
  if (!(list)->data) {                                          \
    error(EXIT_FAILURE, "Cannot allocate sequence list");       \
  }                                                             \
                                                                \
  (list)->length = (size);                                      \
  (list)->next_free = 0;                                        \
  (list)->current = 0;                                          \
} while (0)

#define list_reset (list)                       \
  do {                                          \
    (list)->current = 0;                        \
  } while (0)

void list_append(struct sq_list *list, void *value);
void *list_next(struct sq_list *list);

#endif
