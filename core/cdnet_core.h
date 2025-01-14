/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDNET_CORE_H__
#define __CDNET_CORE_H__

#include "cdbus.h"
#include "cdnet.h"

#ifndef CDN_INTF_MAX
#define CDN_INTF_MAX            1
#endif

struct _cdn_ns;

typedef struct {
    list_node_t     node;
    struct _cdn_ns  *ns;        // cdn_ns_t
    uint16_t        port;
    list_head_t     rx_head;
    bool            tx_only;
} cdn_sock_t;

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
    list_head_t     *free_pkt;
    list_head_t     *free_frm;
    list_head_t     socks;
    cdn_intf_t      intfs[CDN_INTF_MAX];
    cdn_pkt_t       *rx_tmp;
} cdn_ns_t; // name space


cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt); // set _s_mac, _d_mac
int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt);
int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt);

int cdn_sock_bind(cdn_sock_t *sock);
int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt);
cdn_pkt_t *cdn_sock_recvfrom(cdn_sock_t *sock);

void cdn_routine(cdn_ns_t *ns);
void cdn_init_ns(cdn_ns_t *ns, list_head_t *free_pkt, list_head_t *free_frm);
int cdn_add_intf(cdn_ns_t *ns, cd_dev_t *dev, uint8_t net, uint8_t mac);


static inline void cdn_pkt_prepare(cdn_sock_t *sock, cdn_pkt_t *pkt)
{
    pkt->src.port = sock->port;
    if (pkt->dst.addr[0] == 0x10) { // localhost
        pkt->dat = pkt->frm->dat + 3;
    } else {
        cdn_route(sock->ns, pkt);
        pkt->dat = pkt->frm->dat + 3 + cdn_hdr_size_pkt(pkt);
    }
}

static inline cdn_pkt_t *cdn_pkt_alloc(cdn_ns_t *ns)
{
    cd_frame_t *frame = cd_list_get(ns->free_frm);
    if (!frame)
        return NULL;
    cdn_pkt_t *pkt = cdn_list_get(ns->free_pkt);
    if (!pkt) {
        cd_list_put(ns->free_frm, frame);
        return NULL;
    }
    memset(pkt, 0, sizeof(cdn_pkt_t));
    pkt->frm = frame;
    return pkt;
}

static inline void cdn_pkt_free(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    if (pkt->frm) {
        cd_list_put(ns->free_frm, pkt->frm);
        pkt->frm = NULL;
    }
    cdn_list_put(ns->free_pkt, pkt);
}

#endif
