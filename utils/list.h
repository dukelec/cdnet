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

void list_pick(list_head_t *head, list_node_t *pre_node, list_node_t *node);
int list_len(list_head_t *head);


#define list_for_each(head, pre, pos) \
    for (pre = NULL, pos = (head)->first; pos != NULL; \
         pre = pos, pos = pos ? (pos)->next : NULL)

static inline void list_head_init(list_head_t *head)
{
    head->first = NULL;
    head->last = NULL;
}

#endif

