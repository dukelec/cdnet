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
    #define LINE_LEN    80
#endif
#ifndef DBG_LEN
    #define DBG_LEN     60
#endif

typedef struct {
    list_node_t node;
    uint8_t data[LINE_LEN];
    int len;
} dbg_node_t;


static dbg_node_t dbg_alloc[DBG_LEN];

static list_head_t dbg_free = {0};
static list_head_t dbg_tx = {0};
static int dbg_lost_cnt = 0;


// for dprintf

void _dprintf(char* format, ...)
{
    dbg_node_t *buf = list_get_entry_it(&dbg_free, dbg_node_t);
    if (buf) {
        va_list arg;
        va_start (arg, format);
        // WARN: stack may not enough for reentrant
        buf->len = vsnprintf((char *)buf->data, LINE_LEN, format, arg);
        va_end (arg);
        list_put_it(&dbg_tx, &buf->node);
    } else {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
    }
}

void dputs(char *str)
{
    dbg_node_t *buf = list_get_entry_it(&dbg_free, dbg_node_t);
    if (buf) {
        buf->len = strlen(str);
        memcpy(buf->data, str, buf->len);
        list_put_it(&dbg_tx, &buf->node);
    }
}

void dhtoa(uint32_t val, char *buf)
{
    const char tlb[] = "0123456789abcdef";
    int i;

    for (i = 0; i < 8; i++) {
        buf[7 - i] = tlb[val & 0xf];
        val >>= 4;
    }
    buf[8] = '\0';
}

void debug_init(void)
{
    int i;
    for (i = 0; i < DBG_LEN; i++)
        list_put(&dbg_free, &dbg_alloc[i].node);
}

void debug_flush(void)
{
    static int dbg_lost_last = 0;

    while (true) {
#ifdef DBG_TX_IT
        static dbg_node_t *cur_buf = NULL;
        if (!uart_transmit_is_ready(&debug_uart))
            return;
        if (cur_buf)
            list_put_it(&dbg_free, &cur_buf->node);
#endif

        if (dbg_lost_last != dbg_lost_cnt) {
            _dprintf("#: dbg lost: %d -> %d\n", dbg_lost_last, dbg_lost_cnt);
            dbg_lost_last = dbg_lost_cnt;
        }

        dbg_node_t *buf = list_get_entry_it(&dbg_tx, dbg_node_t);
        if (!buf)
            break;
#ifdef DBG_TX_IT
        uart_transmit_it(&debug_uart, buf->data, buf->len);
        cur_buf = buf;
#else
        uart_transmit(&debug_uart, buf->data, buf->len);
        list_put_it(&dbg_free, &buf->node);
#endif
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

