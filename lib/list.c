/*
 * Copyright (C) 2014-2015 Fabien Parent. All rights reserved.
 * Author: Fabien Parent <parent.f@gmail.com>
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <phabos/list.h>

void list_init(struct list_head *head)
{
    head->prev = head->next = head;
}

bool list_is_empty(struct list_head *head)
{
    return head == head->next;
}

void list_add(struct list_head *head, struct list_head *node)
{
    node->next = head;
    node->prev = head->prev;

    head->prev->next = node;
    head->prev = node;
}

void list_sorted_add(struct list_head *head, struct list_head *node,
                     list_node_compare_t compare)
{
    struct list_head *new_head = head;

    list_foreach(head, iter) {
        if (compare(node, iter) >= 0)
            continue;

        new_head = iter;
        break;
    }

    list_add(new_head, node);
}

void list_del(struct list_head *head)
{
    head->prev->next = head->next;
    head->next->prev = head->prev;
    head->prev = head->next = head;
}

void list_rotate_anticlockwise(struct list_head *head)
{
    struct list_head *first = head->next;
    list_del(head);
    list_add(first->next, head);
}

void list_rotate_clockwise(struct list_head *head)
{
    struct list_head *last = head->prev;
    list_del(head);
    list_add(last, head);
}
