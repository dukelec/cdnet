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

#include "cd_utils.h"
#include "cdbus.h"
#include "cdnet.h"
#include "cd_list.h"

#ifndef CDN_INTF_MAX
#define CDN_INTF_MAX            1
#endif

#ifdef CDN_IRQ_SAFE
#define cdn_pkt_get(head)       list_get_entry_it(head, cdn_pkt_t)
#define cdn_list_put            list_put_it
#define cdn_list_put_begin      list_put_begin_it
#elif !defined(CDN_USER_LIST)
#define cdn_pkt_get(head)       list_get_entry(head, cdn_pkt_t)
#define cdn_list_put            list_put
#define cdn_list_put_begin      list_put_begin
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
    list_head_t     *free_pkts;
    list_head_t     socks;
    cdn_intf_t      intfs[CDN_INTF_MAX];
} cdn_ns_t; // name space


cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt); // set _s_mac, _d_mac
int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt);
int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt);

int cdn_sock_bind(cdn_sock_t *sock);
int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt);
cdn_pkt_t *cdn_sock_recvfrom(cdn_sock_t *sock);

void cdn_routine(cdn_ns_t *ns);
void cdn_init_ns(cdn_ns_t *ns, list_head_t *free_head);
int cdn_add_intf(cdn_ns_t *ns, cd_dev_t *dev, uint8_t net, uint8_t mac);

#endif
