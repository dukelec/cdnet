/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 * Reference: https://electronics.stackexchange.com/questions/206113/
 *                          how-do-i-use-the-printf-function-on-stm32
 */

#include "cd_utils.h"
#include "cd_list.h"

extern uart_t debug_uart;

#ifndef DBG_STR_LEN
    #define DBG_STR_LEN 80 // without '\0'
#endif
#ifndef DBG_LEN
    #define DBG_LEN     60
#endif

typedef struct {
    list_node_t node;
    uint8_t data[DBG_STR_LEN + 1];
    int len;
} dbg_node_t;


static dbg_node_t dbg_alloc[DBG_LEN];

static list_head_t dbg_free = {0};
static list_head_t dbg_tx = {0};
static int dbg_lost_cnt = 0;


// for d_printf

void _dprintf(char* format, ...)
{
    static dbg_node_t *buf_cont = NULL;
    dbg_node_t *buf;
    uint32_t flags;
    va_list arg;

    local_irq_save(flags);
    if (buf_cont) {
        buf = buf_cont;
        buf_cont = NULL;
    } else {
        buf = list_get_entry(&dbg_free, dbg_node_t);
        if (!buf) {
            dbg_lost_cnt++;
            local_irq_restore(flags);
            return;
        }
        buf->len = 0;
    }
    local_irq_restore(flags);

    va_start (arg, format);
    // WARN: stack may not enough for reentrant
    // NOTE: size include '\0', return value not include '\0'
    int tgt_len = vsnprintf((char *)buf->data + buf->len, DBG_STR_LEN + 1 - buf->len, format, arg);
    buf->len += min(DBG_STR_LEN - buf->len, tgt_len);
    va_end (arg);

    local_irq_save(flags);
    if (buf->data[buf->len - 1] == '\n' || buf->len == DBG_STR_LEN || buf_cont)
        list_put(&dbg_tx, &buf->node);
    else
        buf_cont = buf;
    local_irq_restore(flags);
}

void _dputs(char *str)
{
    dbg_node_t *buf = list_get_entry_it(&dbg_free, dbg_node_t);
    if (buf) {
        buf->len = strlen(str);
        memcpy(buf->data, str, buf->len);
        list_put_it(&dbg_tx, &buf->node);
    } else {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
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

void debug_uart_init(void)
{
    int i;
    for (i = 0; i < DBG_LEN; i++)
        list_put(&dbg_free, &dbg_alloc[i].node);
}

void debug_flush(bool wait_empty)
{
    static int dbg_lost_last = 0;

    while (true) {
#ifdef DBG_TX_IT
        static dbg_node_t *cur_buf = NULL;
        if (wait_empty) {
            while (!dbg_transmit_is_ready(&debug_uart));
        } else {
            if (!dbg_transmit_is_ready(&debug_uart))
                return;
        }
        if (cur_buf) {
            list_put_it(&dbg_free, &cur_buf->node);
            cur_buf = NULL;
        }
#endif

        dbg_node_t *buf = list_get_entry_it(&dbg_tx, dbg_node_t);
        if (!buf) {
            if (dbg_lost_last != dbg_lost_cnt) {
                _dprintf("#: dbg lost: %d -> %d\n", dbg_lost_last, dbg_lost_cnt);
                dbg_lost_last = dbg_lost_cnt;
            }
            return;
        }
#ifdef DBG_TX_IT
        dbg_transmit_it(&debug_uart, buf->data, buf->len);
        cur_buf = buf;
#else
        dbg_transmit(&debug_uart, buf->data, buf->len);
        list_put_it(&dbg_free, &buf->node);
#endif
    }
}

// for printf

int _write(int file, char *data, int len)
{
   dbg_transmit(&debug_uart, (uint8_t *)data, len);
   return len;
}
