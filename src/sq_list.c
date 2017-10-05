#include <stdlib.h>

#include "error.h"
#include "sq_list.h"

void init_sq_list(struct sq_list *list, int size) {
  assert(list && (size > 0));

  list->data = malloc(sizeof(void *) * size);
  if (!list->data) {
    error(EXIT_FAILURE, "Cannot allocate sequence list");
  }

  list->length = size;
  list->next_free = 0;
  list->current = 0;
}

void *list_current(struct sq_list *list) {
    if (list->current < list->next_free) {
      return list->data[list->current];
    }

    return NULL;                              
}

void list_append(struct sq_list *list, void *value) {
  if (list->next_free == list->length) {
    int size = list->length << 1;

    if (!(list = realloc(list, size))) {
      error(EXIT_FAILURE, "Cannot extend sequence list");
    }
    list->length = size;
  }

  list->data[list->next_free++] = value;
}
