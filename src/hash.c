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
    if (bktp->key == '\0') { /* not insert value yet */
      if (is_empty) {
        *is_empty = 1;
      }
      return bktp;
    } else if (!strcmp(key, bktp->key)) {
      if (is_empty) {
        *is_empty = 0;
      }
      return bktp;
    }
  }

  return NULL;
}

static struct hash_table *rehash(struct hash_table *ht) {
  unsigned int size = ht->size << 1;
  struct hash_table *nht;
  struct bucket *bktp, *nbktp;
  unsigned int i;

  init_hash_table(&nht, size);

  for (i = 0; i < ht->size; i++) {
    bktp = &ht->buckets[i];
    if (bktp->key != '\0') {
      nbktp = hash_table_find_bucket(nht, bktp->key, NULL);

      nbktp->key = bktp->key;
      nbktp->value = bktp->value;
      nht->free--;
    }
  }

  free(ht);

  return nht;
}

void init_hash_table(struct hash_table **ht, unsigned int size) {
    /* size must be non-zere and power of 2 */
    assert((size != 0) && ((size & (~size + 1)) == size));

    *ht = calloc(1, sizeof(struct hash_table) + sizeof(struct bucket) * size);
    if (!*ht) {
      error(EXIT_FAILURE, "Cannot allocate hash table");
    }

    (*ht)->size = size;
    (*ht)->free = size;
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

    /* TODO: rename hash_table_bucket_init to hash_table_init_bucket_value
     * and set bucket key here to avoid redundant codes */
    init_func(bktp, key);
    ht->free--;

    return bktp->value;
  }
}

void *hash_table_current(struct hash_table *ht) {
  while (ht->current < ht->size) {
    if (ht->buckets[ht->current].key) {
      return ht->buckets[ht->current].value;
    }
    ht->current++;
  }

  return NULL;  
}
