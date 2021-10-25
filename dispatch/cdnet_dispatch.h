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
#ifdef CDN_RB_TREE
#include "rbtree.h"
#endif

#ifndef CDN_INTF_MAX
#define CDN_INTF_MAX            1
#endif

#ifndef CDN_ROUTE_MAX
#define CDN_ROUTE_MAX           1
#endif
#ifndef CDN_ROUTE_M_MAX
#define CDN_ROUTE_M_MAX         0
#endif

#ifndef CDN_TGT_MAX
#define CDN_TGT_MAX             4
#endif

#ifndef CDN_L2_P0_RPT_PORT
#define CDN_L2_P0_RPT_PORT      CDN_DEF_PORT
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

struct _cdn_ns;

typedef struct {
#ifdef CDN_RB_TREE
    rb_node_t       node;
#else
    list_node_t     node;
#endif
    struct _cdn_ns  *ns;        // cdn_ns_t
    uint16_t        port;
    list_head_t     rx_head;
    bool            tx_only;
} cdn_sock_t;

#ifdef CDN_SEQ
typedef struct {
    uint8_t         net;        // src net
    uint8_t         mac;        // src mac
    uint8_t         rx_seq;     // seq value, 0xff indicate not use
} cdn_tgt_t; // remote device target
#endif

typedef struct {
    cd_dev_t       *dev;

    // interface address
    uint8_t         net;
    uint8_t         mac;

#ifdef CDN_L0_C                 // L0 role central
    uint8_t         _l0_lp;     // last_port
#endif
} cdn_intf_t;

typedef struct _cdn_ns {
    list_head_t     *free_pkts;
#ifdef CDN_SEQ
    cdn_tgt_t       tgts[CDN_TGT_MAX];
#endif
#ifdef CDN_RB_TREE
    rb_root_t       socks;
#else
    list_head_t     socks;
#endif
#ifdef CDN_L2
    list_head_t     l2_rx;
#endif
    cdn_intf_t      intfs[CDN_INTF_MAX];        //            <--. (search intf)
                                                //               |
    uint32_t        routes[CDN_ROUTE_MAX];      // 0:remote_net:net:mac       // index 0 is default gateway
#if CDN_ROUTE_M_MAX > 0
    uint32_t        routes_m[CDN_ROUTE_M_MAX];  //        MH:ML:net:is_remote // support multiple identical MH:ML
#endif

    cdn_sock_t      sock0;                      // port0 service
} cdn_ns_t; // name space


#ifdef CDN_SEQ
cdn_tgt_t *cdn_tgt_search(cdn_ns_t *ns, uint8_t net, uint8_t mac);
#endif
cdn_sock_t *cdn_sock_search(cdn_ns_t *ns, uint16_t port);
int cdn_sock_insert(cdn_sock_t *sock);
cdn_intf_t *cdn_intf_search(cdn_ns_t *ns, uint8_t net, bool route, int *r_idx);
int cdn_mcast_search(cdn_ns_t *ns, uint16_t mid, int start);
cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt, int start_idx, int *cur_idx); // set _s_mac, _d_mac
int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt);
int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt);

int cdn_sock_bind(cdn_sock_t *sock);
int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt);
cdn_pkt_t *cdn_sock_recvfrom(cdn_sock_t *sock);

void cdn_routine(cdn_ns_t *ns);
void cdn_init_ns(cdn_ns_t *ns, list_head_t *free_head);
int cdn_add_intf(cdn_ns_t *ns, cd_dev_t *dev, uint8_t net, uint8_t mac);

#endif
