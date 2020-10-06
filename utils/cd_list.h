/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CD_LIST_H__
#define __CD_LIST_H__

typedef struct list_node {
   struct list_node *next;
} list_node_t;

typedef struct {
    list_node_t *first;
    list_node_t *last;
    uint32_t    len;
} list_head_t;


list_node_t *list_get(list_head_t *head);
void list_put(list_head_t *head, list_node_t *node);

list_node_t *list_get_last(list_head_t *head);
void list_put_begin(list_head_t *head, list_node_t *node);
void list_pick(list_head_t *head, list_node_t *pre, list_node_t *node);
void list_move_begin(list_head_t *head, list_node_t *pre, list_node_t *node);


#define list_entry(ptr, type)                                   \
    container_of(ptr, type, node)

#define list_entry_safe(ptr, type) ({                           \
        list_node_t *__ptr = (ptr);                             \
        __ptr ? container_of(__ptr, type, node) : NULL;         \
    })

#define list_get_entry(head, type)                              \
        list_entry_safe(list_get(head), type)

#ifdef CD_LIST_IT
#define list_get_entry_it(head, type)                           \
        list_entry_safe(list_get_it(head), type)
#endif

#define list_for_each(head, pre, pos)                           \
    for (pre = NULL, pos = (head)->first; pos != NULL;          \
         pre = pos, pos = (pos ? (pos)->next : (head)->first))
// you can remove a node during the loop:
//      list_pick(head, pre, pos);
//      pos = pre;

// read only version:
#define list_for_each_ro(head, pos)                             \
    for (pos = (head)->first; pos != NULL;                      \
         pos = (pos ? (pos)->next : (head)->first))

#define list_head_init(head)                                    \
    memset(head, 0, sizeof(list_head_t))


#ifdef CD_LIST_IT

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

#endif // CD_LIST_IT

#endif
