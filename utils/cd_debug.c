/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 * Reference: https://electronics.stackexchange.com/questions/206113/
 *                          how-do-i-use-the-printf-function-on-stm32
 */

#include "cd_utils.h"
#include "cd_list.h"
#include "cdnet_dispatch.h"

extern uart_t debug_uart;

#ifndef DBG_STR_LEN
    #define DBG_STR_LEN 200
#endif
#ifndef DBG_MIN_PKT
    #define DBG_MIN_PKT 4
#endif

static bool *dbg_en = NULL;
static cd_sockaddr_t *dbg_dst = NULL;
static cdnet_socket_t sock_dbg = { .port = 9 };

static int dbg_lost_cnt = 0;


// for dprintf

void _dprintf(char* format, ...)
{
    if (!*dbg_en)
        return;

    if (cdnet_free_pkts.len < DBG_MIN_PKT) {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
        return;
    }

    cdnet_packet_t *pkt = cdnet_packet_get(&cdnet_free_pkts);
    if (pkt) {
        va_list arg;
        va_start (arg, format);
        // WARN: stack may not enough for reentrant
        pkt->dat[0] = 0;
        pkt->len = vsnprintf((char *)pkt->dat + 1, DBG_STR_LEN, format, arg) + 1;
        va_end (arg);
        pkt->dst = *dbg_dst;
        cdnet_socket_sendto(&sock_dbg, pkt);
    }
}

void _dputs(char *str)
{
    if (!*dbg_en)
        return;

    if (cdnet_free_pkts.len < DBG_MIN_PKT) {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
        return;
    }

    cdnet_packet_t *pkt = cdnet_packet_get(&cdnet_free_pkts);
    if (pkt) {
        pkt->dat[0] = 0;
        pkt->len = strlen(str) + 1;
        memcpy(pkt->dat + 1, str, pkt->len - 1);
        pkt->dst = *dbg_dst;
        cdnet_socket_sendto(&sock_dbg, pkt);
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

void debug_init(bool *en, cd_sockaddr_t *dst)
{
    dbg_en = en;
    dbg_dst = dst;
    cdnet_socket_bind(&sock_dbg, NULL);
}

void debug_flush(void)
{
    static int dbg_lost_last = 0;

    if (dbg_lost_last != dbg_lost_cnt && cdnet_free_pkts.len >= DBG_MIN_PKT) {
        _dprintf("#: dbg lost: %d -> %d\n", dbg_lost_last, dbg_lost_cnt);
        dbg_lost_last = dbg_lost_cnt;
    }
}

// for printf

int _write(int file, char *data, int len)
{
   if (file != STDOUT_FILENO && file != STDERR_FILENO) {
      errno = EBADF;
      return -1;
   }
   dbg_transmit(&debug_uart, (uint8_t *)data, len);
   return len;
}
