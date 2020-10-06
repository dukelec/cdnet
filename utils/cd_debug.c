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
#include "cdnet_dispatch.h"

extern uart_t debug_uart;

#ifndef DBG_STR_LEN
    #define DBG_STR_LEN 200 // without '\0'
#endif
#ifndef DBG_MIN_PKT
    #define DBG_MIN_PKT 4
#endif

static bool *dbg_en = NULL;
static cdn_sockaddr_t *dbg_dst = NULL;
static cdn_sock_t sock_dbg = { .port = 9 };

static int dbg_lost_cnt = 0;


// for d_printf

void _dprintf(char* format, ...)
{
    static cdn_pkt_t *pkt_cont = NULL;
    cdn_pkt_t *pkt;
    uint32_t flags;
    va_list arg;

    if (!dbg_en || !*dbg_en)
        return;

    local_irq_save(flags);

    if (sock_dbg.ns->free_pkts.len < DBG_MIN_PKT) {
        dbg_lost_cnt++;
        local_irq_restore(flags);
        return;
    }

    if (pkt_cont) {
        pkt = pkt_cont;
        pkt_cont = NULL;
        local_irq_restore(flags);
    } else {
        local_irq_restore(flags);
        pkt = cdn_pkt_get(&sock_dbg.ns->free_pkts);
        if (pkt) {
            pkt->dat[0] = 0x40;
            pkt->len = 1;
            pkt->dst = *dbg_dst;
        } else {
            return;
        }
    }

    va_start (arg, format);
    // WARN: stack may not enough for reentrant
    // NOTE: size include '\0', return value not include '\0'
    int tgt_len = vsnprintf((char *)pkt->dat + pkt->len, DBG_STR_LEN + 2 - pkt->len, format, arg);
    pkt->len += min(DBG_STR_LEN + 1 - pkt->len, tgt_len); // + 1 for dat[0]

    va_end (arg);

    local_irq_save(flags);
    if (pkt->dat[pkt->len - 1] == '\n' || pkt->len == DBG_STR_LEN + 1 || pkt_cont) {
        local_irq_restore(flags);
        cdn_sock_sendto(&sock_dbg, pkt);
    } else {
        pkt_cont = pkt;
        local_irq_restore(flags);
    }
}

void _dputs(char *str)
{
    if (!dbg_en || !*dbg_en)
        return;

    if (sock_dbg.ns->free_pkts.len < DBG_MIN_PKT) {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
        return;
    }

    cdn_pkt_t *pkt = cdn_pkt_get(&sock_dbg.ns->free_pkts);
    if (pkt) {
        pkt->dat[0] = 0x40;
        pkt->len = strlen(str) + 1;
        memcpy(pkt->dat + 1, str, pkt->len - 1);
        pkt->dst = *dbg_dst;
        cdn_sock_sendto(&sock_dbg, pkt);
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

void debug_init(cdn_ns_t *ns, cdn_sockaddr_t *dst, bool *en)
{
    sock_dbg.ns = ns;
    dbg_dst = dst;
    dbg_en = en;
    cdn_sock_bind(&sock_dbg);
}

void debug_flush(void)
{
    static int dbg_lost_last = 0;

    if (dbg_lost_last != dbg_lost_cnt && sock_dbg.ns->free_pkts.len >= DBG_MIN_PKT) {
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
