/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDNET_H__
#define __CDNET_H__

#include "common.h"

#ifndef CDNET_DEF_PORT
#define CDNET_DEF_PORT      0xcdcd
#endif

#ifndef SEQ_REC_MAX
#define SEQ_REC_MAX         3
#endif

#ifndef SEQ_TIMEOUT
#define SEQ_TIMEOUT         5 // ms
#endif


#define CDNET_L0            0
#define CDNET_L1            1
#define CDNET_L2            2

#define HDR_L1_L2           (1 << 7)
#define HDR_L2              (1 << 6)

#define HDR_L0_REPLY        (1 << 6)
#define HDR_L0_SHARE        (1 << 5)

#define HDR_L1_MULTI_NET    (1 << 5)
#define HDR_L1_MULTICAST    (1 << 4)
#define HDR_L1_SEQ_NUM      (1 << 3)

#define HDR_L2_FRAGMENT     (1 << 5)
#define HDR_L2_FRAGMENT_END (1 << 4)
#define HDR_L2_SEQ_NUM      (1 << 3)
#define HDR_L2_COMPRESSED   (1 << 2)


#define ERR_ASSERT          -1


// cd: base class for MAC layer
// function pointer implement by child class

typedef struct {
    list_node_t node;
    uint8_t     dat[260]; // max size for cdbus through uart
} cd_frame_t;

typedef struct cd_intf {

    list_node_t *(* get_free_node)(struct cd_intf *cd_intf);
    list_node_t *(* get_rx_node)(struct cd_intf *cd_intf);
    void (* put_free_node)(struct cd_intf *cd_intf, list_node_t *node);
    void (* put_tx_node)(struct cd_intf *cd_intf, list_node_t *node);

    // cdbus has two bond rates
    void  (* set_bond_rate)(struct cd_intf *intf, uint16_t, uint16_t);
    void  (* get_bond_rate)(struct cd_intf *intf, uint16_t *, uint16_t *);

    // 255 for promiscuous mode
    void    (* set_filter)(struct cd_intf *intf, uint8_t filter);
    uint8_t (* get_filter)(struct cd_intf *intf);

    void    (* flush)(struct cd_intf *intf);
} cd_intf_t;


typedef struct {
    list_node_t node;

    uint8_t     level;

    bool        is_seq;
    uint8_t     seq_num;
    bool        req_ack;
    uint32_t    send_time;

    bool        is_multi_net;
    bool        is_multicast;

    uint8_t     src_mac;
    uint8_t     dst_mac;
    uint8_t     src_addr[2]; // [net, mac]
    uint8_t     dst_addr[2];

    uint16_t    multicast_id;

    uint16_t    src_port;
    uint16_t    dst_port;

    // level 2 only
    bool        is_fragment;
    bool        is_fragment_end;
    bool        is_compressed;

    int         dat_len;
    uint8_t     dat[252];
} cdnet_packet_t;


typedef struct {
    list_node_t node;
    bool        is_multi_net;
    uint8_t     net;
    uint8_t     mac;

    uint8_t     seq_num;

    // for tx only
    list_head_t pend_head;
    uint8_t     pend_cnt; // items in pend_head
    uint8_t     send_cnt;
    cdnet_packet_t *p1_req;
} seq_rec_t;

typedef struct {
    uint8_t     mac;
    uint8_t     net;
    uint8_t     l0_last_port;

    list_head_t *free_head;
    list_head_t rx_head;
    list_head_t tx_head;

    seq_rec_t   seq_rec_alloc[SEQ_REC_MAX];
    list_head_t seq_rx_head;
    list_head_t seq_tx_head;

    cd_intf_t   *cd_intf;
} cdnet_intf_t;


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
    cd_intf_t *cd_intf, uint8_t mac);


// helper

void cdnet_exchange_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt);

void cdnet_rx(cdnet_intf_t *intf);
void cdnet_tx(cdnet_intf_t *intf);

#endif

