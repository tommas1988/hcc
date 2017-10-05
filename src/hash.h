#ifndef __HCC_HASH_H
#define __HCC_HASH_H

#include <stdlib.h>
#include <assert.h>
#include "error.h"

struct bucket {
  char *key;
  void *value;
};

struct hash_table {
  unsigned int size;
  unsigned int free;
  unsigned int current;
  struct bucket buckets[];
};

typedef void (*hash_table_bucket_init) (struct bucket *bktp, const char *key);

void init_hash_table(struct hash_table **ht, unsigned int size);
void *hash_table_find_with_add(struct hash_table *ht, const char *key, hash_table_bucket_init init_func);
#define hash_table_find(ht, key) hash_table_find_with_add((ht), (key), NULL)
#define hash_table_reset(ht) do { (ht)->current = 0; } while (0)
#define hash_table_next(ht) do { (ht)->current++; } while (0)
void *hash_table_current(struct hash_table *ht);

#endif
