#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "hash.h"

/*
 * DJBX33A
 */
static unsigned long str2hash(const char *key) {
  const char *p;
  unsigned long hash = 0;

  for (p = key; *p != '\0'; p++) {
    hash = hash * 33 + *p;
  }

  return hash;
}

/*
 * Double Hashing
 */
static int hash_func(struct hash_table *ht, unsigned long hash, unsigned int i) {
  int h1 = hash % ht->size;
  int h2 = h1 & 1 ? h1 : (h1 > 0 ? h1 - 1 : h1 + 1);

  return (h1 + i * h2) % ht->size;
}

static struct bucket *hash_table_find_bucket(struct hash_table *ht, const char *key, int *is_empty) {
  unsigned int i, idx;
  unsigned long hash = str2hash(key);
  struct bucket *bktp;

  for (i = 0; i < ht->size; i++) {
    idx = hash_func(ht, hash, i);
    bktp = &ht->buckets[idx];
    if (bktp->key[0] == '\0') { /* not insert value yet */
      *is_empty = 1;
      return bktp;
    } else if (!strcmp(key, bktp->key)) {
      *is_empty = 0;
      return bktp;
    }
  }

  return NULL;
}

static struct hash_table *rehash(struct hash_table *ht) {
  unsigned int size = ht->size << 1;
  struct hash_table *nht;
  struct bucket *bktp;
  unsigned int i;

  init_hash_table(nht, size);

  for (i = 0; i < ht->size; i++) {
    bktp = &ht->buckets;
    if (bktp->key[0] != '\0') {
      hash_table_add(nht, bktp->key, bktp->value);
    }
  }

  free(ht);
}

void *hash_table_find_with_add(struct hash_table *ht, const char *key, hash_table_bucket_init init_func) {
  int is_empty;
  struct bucket *bktp;

  bktp = hash_table_find_bucket(ht, key, &is_empty);

  if (bktp && !is_empty) {
    return bktp->value;
  } else if (!init_func) {
    return NULL;
  } else {
    if (!bktp) {
      assert(ht->free == 0);
      rehash(ht);
    }

    init_func(bktp, key);
    ht->free--;
  }
}
