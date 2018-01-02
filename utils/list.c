/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "list.h"


list_node_t *list_get(list_head_t *head)
{
    list_node_t *node = NULL;
    if (head->first != NULL) {
        node = head->first;
        head->first = node->next;
        if (!node->next)
            head->last = NULL;
        node->next = NULL;
    }
    return node;
}

void list_put(list_head_t *head, list_node_t *node)
{
    if (head->last)
        head->last->next = node;
    else
        head->first = node;
    head->last = node;
    node->next = NULL;
}

void list_pick(list_head_t *head, list_node_t *pre_node, list_node_t *node)
{
    if (pre_node)
        pre_node->next = node->next;
    else
        head->first = node->next;
    if (!node->next)
        head->last = NULL;
    node->next = NULL;
}

int list_len(list_head_t *head)
{
    int ret_val = 0;
    list_node_t *node = head->first;
    while (node) {
        ret_val++;
        node = node->next;
    }
    return ret_val;
}

