#ifndef __HCC_HASH_H
#define __HCC_HASH_H

#include <stdlib.h>
#include <assert.h>
#include "error.h"

struct bucket {
  char *key;
  void *value;
}

struct hash_table {
  unsigned int size;
  unsigned int free;
  bucket buckets[];
};

typedef void (*update_bucket_func)(struct bucket *bktp, const char *key, void *value);

#define init_hash_table (ht, size)                                      \
  do {                                                                  \
    /* size must be non-zere and power of 2 */                          \
    assert(((size) != 0) && ((size) & (~(size) + 1) == (size)));        \
                                                                        \
    ht = calloc(1, sizeof(struct hash_table) + sizeof(struct bucket) * (size)); \
    if (!ht) {                                                          \
      error(EXIT_FAILURE, "Cannot allocate hash table");                \
    }                                                                   \
                                                                        \
    ht->size = size;                                                    \
    ht->free = size;                                                    \
  } while (0)

void *hash_table_find(struct hash_table *ht, const char *key);
void hash_table_add_or_update(struct hash_table *ht, const char *key, void *value, update_bucket_func func);

#endif
