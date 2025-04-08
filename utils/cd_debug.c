/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet_core.h"

#ifndef CD_DBG_STR_LEN
    #define CD_DBG_STR_LEN 200 // without '\0'
#endif
#ifndef CD_DBG_MIN_PKT
    #define CD_DBG_MIN_PKT 10
#endif

static bool *dbg_en = NULL;
static cdn_sockaddr_t *dbg_dst = NULL;
static cdn_sock_t sock_dbg = { .port = 9, .tx_only = true };

static list_head_t dbg_pend = { 0 };
static int dbg_lost_cnt = 0;


// for d_printf

void _dprintf(char* format, ...)
{
    static cdn_pkt_t *pkt_cont = NULL;
    cdn_pkt_t *pkt;
    uint32_t flags;
    va_list arg;

    if (!dbg_en || !*dbg_en) {
        va_start (arg, format);
        vprintf(format, arg);
        va_end (arg);
        return;
    }

    if (sock_dbg.ns->free_pkt->len == 0)
        return;

    local_irq_save(flags);

    if (sock_dbg.ns->free_pkt->len < CD_DBG_MIN_PKT || sock_dbg.ns->free_frm->len < CD_DBG_MIN_PKT) {
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
        pkt = cdn_pkt_alloc(sock_dbg.ns);
        if (pkt) {
            pkt->dst = *dbg_dst;
            cdn_pkt_prepare(&sock_dbg, pkt);
            pkt->dat[0] = 0x40;
            pkt->len = 1;
        } else {
            return;
        }
    }

    va_start (arg, format);
    // WARN: stack may not enough for reentrant
    // NOTE: size include '\0', return value not include '\0'
    int tgt_len = vsnprintf((char *)pkt->dat + pkt->len, CD_DBG_STR_LEN + 2 - pkt->len, format, arg);
    pkt->len += min(CD_DBG_STR_LEN + 1 - pkt->len, tgt_len); // + 1 for dat[0]

    va_end (arg);

    local_irq_save(flags);
    if (pkt->dat[pkt->len - 1] == '\n' || pkt->len == CD_DBG_STR_LEN + 1 || pkt_cont) {
        cdn_list_put(&dbg_pend, pkt);
    } else {
        pkt_cont = pkt;
    }
    local_irq_restore(flags);
}

void _dputs(char *str)
{
    if (!dbg_en || !*dbg_en)
        return;

    if (sock_dbg.ns->free_pkt->len < CD_DBG_MIN_PKT || sock_dbg.ns->free_frm->len < CD_DBG_MIN_PKT) {
        uint32_t flags;
        local_irq_save(flags);
        dbg_lost_cnt++;
        local_irq_restore(flags);
        return;
    }

    cdn_pkt_t *pkt = cdn_pkt_alloc(sock_dbg.ns);
    if (pkt) {
        pkt->dst = *dbg_dst;
        cdn_pkt_prepare(&sock_dbg, pkt);
        pkt->dat[0] = 0x40;
        pkt->len = strlen(str) + 1;
        memcpy(pkt->dat + 1, str, pkt->len - 1);
        cdn_list_put(&dbg_pend, pkt);
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

void debug_flush(bool wait_empty)
{
    static int dbg_lost_last = 0;

    if (dbg_lost_last != dbg_lost_cnt && sock_dbg.ns->free_pkt->len >= CD_DBG_MIN_PKT) {
        _dprintf("#: dbg lost: %d -> %d\n", dbg_lost_last, dbg_lost_cnt);
        dbg_lost_last = dbg_lost_cnt;
    }

    while (true) {
        if (!sock_dbg.ns->intfs[0].dev)
            return;
        cdn_pkt_t *pkt = cdn_list_get(&dbg_pend);
        if (!pkt)
            return;
        cdn_sock_sendto(&sock_dbg, pkt);
    }
}

// for printf

int _write(int file, char *data, int len)
{
   arch_dbg_tx((uint8_t *)data, len);
   return len;
}
