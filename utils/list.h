/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __LIST_H__
#define __LIST_H__

#include "common.h"


typedef struct list_node {
   struct list_node *next;
} list_node_t;

typedef struct {
    list_node_t *first;
    list_node_t *last;
} list_head_t;


list_node_t *list_get(list_head_t *head);
void list_put(list_head_t *head, list_node_t *node);

list_node_t *list_get_last(list_head_t *head);
void list_put_begin(list_head_t *head, list_node_t *node);
void list_pick(list_head_t *head, list_node_t *pre, list_node_t *node);
void list_move_begin(list_head_t *head, list_node_t *pre, list_node_t *node);
int list_len(list_head_t *head);


#define list_entry(ptr, type)                           \
    container_of(ptr, type, node)

#define list_entry_safe(ptr, type) ({                   \
        list_node_t *_ptr = (ptr);                      \
        _ptr ? container_of(_ptr, type, node) : NULL;   \
    })

#define list_get_entry(head, type)                      \
        list_entry_safe(list_get(head), type)

#define list_get_entry_it(head, type)                   \
        list_entry_safe(list_get_it(head), type)

#define list_for_each(head, pre, pos)                   \
    for (pre = NULL, pos = (head)->first; pos != NULL;  \
         pre = pos, pos = pos ? (pos)->next : NULL)


static inline void list_head_init(list_head_t *head)
{
    head->first = NULL;
    head->last = NULL;
}

static inline list_node_t *list_get_it(list_head_t *head)
{
    uint32_t flags;
    list_node_t *node;
    local_irq_save(flags);
    node = list_get(head);
    local_irq_restore(flags);
    return node;
}

static inline void list_put_it(list_head_t *head, list_node_t *node)
{
    uint32_t flags;
    local_irq_save(flags);
    list_put(head, node);
    local_irq_restore(flags);
}

static inline void list_put_begin_it(list_head_t *head, list_node_t *node)
{
    uint32_t flags;
    local_irq_save(flags);
    list_put_begin(head, node);
    local_irq_restore(flags);
}

#endif
