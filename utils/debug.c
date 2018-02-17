/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 * Reference: https://electronics.stackexchange.com/questions/206113/
 *                          how-do-i-use-the-printf-function-on-stm32
 */

#include "common.h"

extern uart_t debug_uart;

#ifndef LINE_LEN
    #define LINE_LEN 80
#endif
#ifndef DBG_LEN
    #define DBG_LEN 60
#endif

typedef struct {
    list_node_t node;
    uint8_t data[LINE_LEN];
    int len;
} dbg_node_t;


static dbg_node_t dbg_alloc[DBG_LEN];

static list_head_t dbg_free = {0};
static list_head_t dbg_tx = {0};


// for dprintf

void _dprintf(char* format, ...)
{
    list_node_t *node;

    while (true) {
        node = list_get_irq_safe(&dbg_free);
        if (node)
            break;
        debug_flush();
    }
    if (node) {
        dbg_node_t *buf = container_of(node, dbg_node_t, node);
        va_list arg;
        va_start (arg, format);
        // WARN: stack may not enough for reentrant
        buf->len = vsnprintf((char *)buf->data, LINE_LEN, format, arg);
        va_end (arg);
        list_put_irq_safe(&dbg_tx, node);
    }
}

void dputs(char *str)
{
    list_node_t *node;

    node = list_get_irq_safe(&dbg_free);
    if (node) {
        dbg_node_t *buf = container_of(node, dbg_node_t, node);
        buf->len = strlen(str);
        memcpy(buf->data, str, buf->len);
        list_put_irq_safe(&dbg_tx, node);
    }
}

void dputhex(int n)
{
    char buf[10];
    const char tlb[] = "0123456789abcdef";
    int i;

    for (i = 0; i < 8; i++) {
        buf[7 - i] = tlb[n & 0xf];
        n >>= 4;
    }
    buf[8] = '\0';
    dputs(buf);
}

void debug_init(void)
{
    int i;
    for (i = 0; i < DBG_LEN; i++)
        list_put(&dbg_free, &dbg_alloc[i].node);
}

void debug_flush(void)
{
    while (true) {
        list_node_t *node = list_get_irq_safe(&dbg_tx);
        if (!node)
            break;
        dbg_node_t *buf = container_of(node, dbg_node_t, node);
        uart_transmit(&debug_uart, buf->data, buf->len);
        list_put_irq_safe(&dbg_free, node);
    }
}

// for printf

int _write(int file, char *data, int len)
{
   if (file != STDOUT_FILENO && file != STDERR_FILENO) {
      errno = EBADF;
      return -1;
   }
   uart_transmit(&debug_uart, (uint8_t *)data, len);
   return len;
}

