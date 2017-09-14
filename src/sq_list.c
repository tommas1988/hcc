#include <stdlib.h>

#include "error.h"
#include "sq_list.h"

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
