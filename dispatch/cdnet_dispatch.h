/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDNET_DISPATH_H__
#define __CDNET_DISPATH_H__

#include "cd_utils.h"
#include "cdbus.h"
#include "cdnet.h"
#include "cd_list.h"
#include "rbtree.h"

#ifndef CDN_INTF
#define CDN_INTF_MAX            1
#endif

#ifndef CDN_ROUTE
#define CDN_ROUTE_MAX           1
#endif

#ifndef CDN_SEQ_ACK_CNT
#define CDN_SEQ_ACK_CNT         3
#endif
#ifndef CDN_SEQ_P0_RETRY_MAX
#define CDN_SEQ_P0_RETRY_MAX    3
#endif
#ifndef CDN_SEQ_TX_PEND_MAX
#define CDN_SEQ_TX_PEND_MAX     6
#endif

#ifndef CDN_SEQ_TIMEOUT
#define CDN_SEQ_TIMEOUT         (50000 / SYSTICK_US_DIV)    // 50 ms
#endif

#ifndef CDN_TGT_GC              // release inactive tgt
#define CDN_TGT_GC              (60000000 / SYSTICK_US_DIV) // 60 s
#endif

#ifdef CDN_IRQ_SAFE
#define cdn_pkt_get(head)       list_get_entry_it(head, cdn_pkt_t)
#define cdn_tgt_get(head)       list_get_entry_it(head, cdn_tgt_t)
#define cdn_list_put            list_put_it
#define cdn_list_put_begin      list_put_begin_it
#elif !defined(CDN_USER_LIST)
#define cdn_pkt_get(head)       list_get_entry(head, cdn_pkt_t)
#define cdn_tgt_get(head)       list_get_entry(head, cdn_tgt_t)
#define cdn_list_put            list_put
#define cdn_list_put_begin      list_put_begin
#endif

#ifdef CDN_TGT
typedef struct {
    list_node_t     node;

    uint16_t        id;         // net:mac or MH:ML
    uint8_t         rx_seq;     // seq value
    uint8_t         tx_seq;
    uint8_t         tx_seq_r;   // read back

    uint32_t        t_last;     // last active time
    uint32_t        t_tx_last;  // last tx time

#ifdef CDN_L0_C                 // L0 role central
    uint8_t         _l0_lp;     // last_port
#endif
#ifdef CDN_L2
    cdn_pkt_t       *_l2_rx;
#endif

    // for tx:

    list_head_t     tgts;       // sub tgts, multicast only

    //   not used for sub tgts:

    list_head_t     tx_wait;
    list_head_t     tx_pend;
    uint8_t         tx_cnt;     // request ack for each SEQ_ACK_CNT
    uint8_t         p0_retry;   // retry cnt
    uint8_t         p0_state;   // 0: idle, 1: get, 2: reset, bit7: finish
} cdn_tgt_t; // remote device target
#endif // CDN_TGT

typedef struct {
    cd_dev_t       *dev;

    // interface address
    uint8_t         net;
    uint8_t         mac;
} cdn_intf_t;

typedef struct {
    list_head_t     free_pkts;
#ifdef CDN_TGT
    list_head_t     free_tgts;
    list_head_t     tgts;
#endif
    rb_root_t       socks;
#ifdef CDN_L2
    list_head_t     l2_rx;
#endif
    cdn_intf_t      intfs[CDN_INTF_MAX];

    uint16_t        route[CDN_ROUTE_MAX]; // net:mac, first is default gateway
} cdn_ns_t; // name space

typedef struct {
    rb_node_t       node;
    cdn_ns_t        *ns;
    uint16_t        port;
    list_head_t     rx_head;
} cdn_sock_t;


int cdn_sock_bind(cdn_sock_t *sock);
int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt);
cdn_pkt_t *cdn_sock_recvfrom(cdn_sock_t *sock);

void cdn_routine(cdn_ns_t *ns);

#endif
