/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "list.h"

// pick first item
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

// append item at end
void list_put(list_head_t *head, list_node_t *node)
{
    if (head->last)
        head->last->next = node;
    else
        head->first = node;
    head->last = node;
    node->next = NULL;
}

list_node_t *list_get_last(list_head_t *head)
{
    list_node_t *pre = NULL;
    list_node_t *node = head->first;

    if (!node)
        return NULL;

    while (node->next) {
        pre = node;
        node = node->next;
    }

    if (pre) {
        pre->next = NULL;
        head->last = pre;
    } else {
        head->first = head->last = NULL;
    }

    return node;
}

void list_put_begin(list_head_t *head, list_node_t *node)
{
    node->next = head->first;
    head->first = node;
    if (!head->last)
        head->last = node;
}

void list_pick(list_head_t *head, list_node_t *pre, list_node_t *node)
{
    if (pre)
        pre->next = node->next;
    else
        head->first = node->next;
    if (!node->next)
        head->last = NULL;
    node->next = NULL;
}

void list_move_begin(list_head_t *head, list_node_t *pre, list_node_t *node)
{
    if (!pre)
        return;

    pre->next = node->next;
    node->next = head->first;
    head->first = node;

    if (head->last == node)
        head->last = pre;
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
