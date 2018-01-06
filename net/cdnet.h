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

#ifndef CDNET_MAX_SIZE
#define CDNET_MAX_SIZE  252
#endif

#ifndef CDNET_BASIC_PORT
#define CDNET_BASIC_PORT  0xcdcd
#endif


// cd: base class for MAC layer
// function pointer implement by child class

typedef struct {
    list_node_t node;
    uint8_t     dat[256 + 3 + 2]; // "3 + 2" for cdbus through uart
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

    bool     	is_local;
    bool		is_multicast;

    uint8_t     src_addr[2];	// net_id and mac for corresponding net
    uint8_t     src_mac;		// local addr
    uint8_t     dst_addr[2];	// last byte is multicast id in multicast
    uint8_t     dst_mac;

    bool 		is_compressed;

    uint8_t     pkt_type;     	// 0: UDP

    uint16_t    src_port;
    uint16_t    dst_port;

    bool        in_fragment;
    uint8_t     frag_cnt;
    uint8_t     *frag_at;

    uint16_t    dat_len;
    uint8_t     dat[CDNET_MAX_SIZE];
} cdnet_packet_t;

typedef struct {
    uint8_t     mac;
    uint8_t     net_id;
    uint8_t     last_basic_port;

    list_head_t *free_head;
    list_head_t rx_head;
    list_head_t tx_head;
    list_head_t rx_frag_head;
    list_head_t tx_frag_head;   // max 1 item

    cd_intf_t   *cd_intf;
} cdnet_intf_t;


#define HDR_STANDARD        (1 << 7)
#define HDR_FULL_ADDR       (1 << 6)
#define HDR_MULTICAST       (1 << 5)
#define HDR_FRAGMENT        (1 << 4)
#define HDR_COMPRESSED      (1 << 3)
#define HDR_FURTHER_PROT    (1 << 2)
#define HDR_SRC_PORT_16     (1 << 1)
#define HDR_DST_PORT_16     (1 << 0)

#define HDR_BASIC_REPLY     (1 << 6)
#define HDR_BASIC_SHARE     (1 << 5)

#define HDR_FRAGMENT_END    (1 << 7)

#define PKT_TYPE_UDP        0
#define PKT_TYPE_TCP        1
#define PKT_TYPE_ICMP       2
#define PKT_TYPE_END        2


#define ERR_ASSERT          -1
#define ERR_PKT_ORDER       -2

#define RET_PKT_NOT_MATCH   1
#define RET_NOT_FINISH      2


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
    cd_intf_t *cd_intf, uint8_t mac);


// helper

void cdnet_exchange_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt);

void cdnet_rx(cdnet_intf_t *intf);
void cdnet_tx(cdnet_intf_t *intf);

#endif

