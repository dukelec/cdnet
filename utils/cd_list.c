/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cd_utils.h"
#include "cd_list.h"

#ifdef LIST_DEBUG
#include <unwind.h>
static void list_check(list_head_t *head);
#endif

// pick first item
list_node_t *list_get(list_head_t *head)
{
    list_node_t *node = NULL;
    if (head->len) {
        node = head->first;
        head->first = node->next;
        if (--head->len == 0)
            head->last = NULL;
    }
#ifdef LIST_DEBUG
    list_check(head);
#endif
    return node;
}

// append item at end
void list_put(list_head_t *head, list_node_t *node)
{
    if (head->len++)
        head->last->next = node;
    else
        head->first = node;
    head->last = node;
    node->next = NULL;
#ifdef LIST_DEBUG
    list_check(head);
#endif
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
    head->len--;

#ifdef LIST_DEBUG
    list_check(head);
#endif
    return node;
}

void list_put_begin(list_head_t *head, list_node_t *node)
{
    node->next = head->first;
    head->first = node;
    if (!head->len++)
        head->last = node;
#ifdef LIST_DEBUG
    list_check(head);
#endif
}

void list_pick(list_head_t *head, list_node_t *pre, list_node_t *node)
{
    if (pre)
        pre->next = node->next;
    else
        head->first = node->next;
    if (--head->len == 0)
        head->last = NULL;
#ifdef LIST_DEBUG
    list_check(head);
#endif
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
#ifdef LIST_DEBUG
    list_check(head);
#endif
}


#ifdef LIST_DEBUG
static _Unwind_Reason_Code trace_fcn(_Unwind_Context *ctx, void *_)
{
    printf("bt: [%08x]\n", _Unwind_GetIP(ctx));
    return _URC_NO_REASON;
}

static void list_check(list_head_t *head)
{
    int len = 0;
    list_node_t *node = head->first;
    list_node_t *pre = NULL;

    while (node) {
        pre = node;
        node = node->next;
        len++;
    }

    if (head->len != len) {
        printf("PANIC: list %p, wrong len: %d, %d\n", head, head->len, len);
        _Unwind_Backtrace(&trace_fcn, NULL);
        while (true);
    }
    if (head->last != pre) {
        printf("PANIC: list %p, wrong head->last: %p, %p, len: %d, %d\n",
                head, head->last, node, head->len, len);
        _Unwind_Backtrace(&trace_fcn, NULL);
        while (true);
    }
}
#endif
