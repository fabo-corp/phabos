/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stddef.h>
#include <stdbool.h>

#include <phabos/assert.h>

#define HASHTABLE_ITERATOR_INIT {0}

struct hashtable_node;
struct hashtable;

typedef unsigned int (*hashtable_hash_fct_t)(struct hashtable *ht, void *key);
typedef int (*hashtable_key_compare_fct_t)(const void *key1, const void *key2);

struct hashtable_iterator {
    unsigned long i;
    void *key;
    void *value;
};

typedef struct hashtable
{
    struct hashtable_node *table;
    size_t size;
    size_t count;
    hashtable_hash_fct_t hash;
    hashtable_key_compare_fct_t compare;
} hashtable_t;

void hashtable_init(hashtable_t *ht, hashtable_hash_fct_t hash,
                    hashtable_key_compare_fct_t compare);
void hashtable_add(hashtable_t *ht, void *key, void *value);
void *hashtable_get(hashtable_t *ht, void *key);
bool hashtable_has(hashtable_t *ht, void *key);
void hashtable_remove(hashtable_t *ht, void *key);
bool hashtable_iterate(hashtable_t *ht, struct hashtable_iterator *iter);

#define hashtable_init_uint(ht) hashtable_init((ht), hash_uint, \
                                               hashtable_key_compare_uint)
#define hashtable_init_string(ht) hashtable_init((ht), hash_string, \
                                                 hashtable_key_compare_string)

unsigned int hash_uint(hashtable_t *ht, void *key);
unsigned int hash_string(hashtable_t *ht, void *key);

int hashtable_key_compare_uint(const void *key1, const void *key2);
int hashtable_key_compare_string(const void *key1, const void *key2);

#endif /* __HASHTABLE_H__ */

