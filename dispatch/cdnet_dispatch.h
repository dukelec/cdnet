/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDNET_DISPATH_H__
#define __CDNET_DISPATH_H__

#include "cd_utils.h"
#include "cdbus.h"
#include "cdnet.h"
#include "cd_list.h"
#include "rbtree.h"

#ifndef EPHEMERAL_BEGIN     // begin ephemeral port
#define EPHEMERAL_BEGIN     32768
#endif

#ifndef SEQ_TX_ACK_CNT
#define SEQ_TX_ACK_CNT      3
#endif
#ifndef SEQ_TX_RETRY_MAX
#define SEQ_TX_RETRY_MAX    3
#endif
#ifndef SEQ_TX_PEND_MAX
#define SEQ_TX_PEND_MAX     6
#endif

#ifndef SEQ_TIMEOUT
#define SEQ_TIMEOUT         (5000 / SYSTICK_US_DIV) // 5 ms
#endif

#ifndef CDNET_MAX_DAT
#define CDNET_MAX_DAT       252
#endif

#ifdef CDNET_IRQ_SAFE
#define cdnet_packet_get(head)  list_get_entry_it(head, cdnet_packet_t)
#define cdnet_list_put          list_put_it
#define cdnet_list_put_begin    list_put_begin_it
#elif !defined(CDNET_USER_LIST)
#define cdnet_packet_get(head)  list_get_entry(head, cdnet_packet_t)
#define cdnet_list_put          list_put
#define cdnet_list_put_begin    list_put_begin
#endif


extern list_head_t cdnet_free_pkts;


typedef struct {
    list_node_t     node;

    cd_sockaddr_t   src;
    cd_sockaddr_t   dst;

    uint8_t         len; // TODO: add support for big packet
    uint8_t         dat[CDNET_MAX_DAT];
} cdnet_packet_t;


typedef struct {
    list_node_t     node;
    uint16_t        addr16; // net:mac
    uint8_t         rx_seq_num;
    uint8_t         tx_seq_num;

    // for tx only
    list_head_t     tx_wait_head;
    list_head_t     tx_pend_head;
    uint8_t         send_cnt; // send ack for each SEQ_TX_ACK_CNT
    uint8_t         p0_retry_cnt;
    cdnet_packet_t  *p0_req;
} cdnet_node_t;

typedef struct {
    cd_dev_t       *dev;

    // interface address
    uint8_t         net;
    uint8_t         mac;

    // nodes head
} cdnet_intf_t;


typedef struct {
    rb_node_t       node;
    uint16_t        port;
    list_head_t     rx_head;
} cdnet_socket_t;


int cdnet_intf_register(cdnet_intf_t *intf);
cdnet_intf_t *cdnet_intf_search(uint8_t net);
cdnet_intf_t *cdnet_route_search(const cd_addr_t *d_addr, uint8_t *d_mac);

void cdnet_intf_init(cdnet_intf_t *intf, cd_dev_t *dev,
        uint8_t net, uint8_t mac);

void cdnet_intf_routine(void);


int cdnet_socket_bind(cdnet_socket_t *sock, cd_sockaddr_t *addr);

int cdnet_socket_sendto(cdnet_socket_t *sock, cdnet_packet_t *pkt);

cdnet_packet_t *cdnet_socket_recvfrom(cdnet_socket_t *sock);

#endif
