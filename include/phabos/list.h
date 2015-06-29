/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#ifndef __LIST_H__
#define __LIST_H__

#include <stdbool.h>
#include <stdint.h>

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

typedef int (*list_node_compare_t)(struct list_head*, struct list_head*);

void list_init(struct list_head *head);
void list_add(struct list_head *head, struct list_head *node);
void list_del(struct list_head *head);
bool list_is_empty(struct list_head *head);
void list_rotate_anticlockwise(struct list_head *head);
void list_rotate_clockwise(struct list_head *head);
void list_sorted_add(struct list_head *head, struct list_head *node,
                     list_node_compare_t compare);

#define list_entry(n, s, f) ((void*) (((uint8_t*) (n)) - offsetof(s, f)))

#define list_foreach(head, iter) \
    for (struct list_head *iter = (head)->next; \
         iter != (head); \
         iter = iter->next)

#define list_foreach_safe(head, iter) \
    for (struct list_head *iter = (head)->next, *niter = iter->next; \
         iter != (head); \
         iter = niter, niter = niter->next)

#define list_first_entry(head, s, f) list_entry((head)->next, s, f)
#define list_last_entry(head, s, f) list_entry((head)->prev, s, f)

#define LIST_INIT(head) { .prev = &head, .next = &head }

#endif /* __LIST_H__ */

