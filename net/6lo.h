/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __6LO_H__
#define __6LO_H__

#include "common.h"

#define LO_ADDR_LL0     0x3
#define LO_ADDR_UGC16   0x6
#define LO_ADDR_UG128   0x0
#define LO_ADDR_UNSP    0x4
#define LO_ADDR_M8      0xb
#define LO_ADDR_M32     0xa
#define LO_ADDR_M128    0x8

#define LO_NH_UDP  0xf4       // C = 'b1
#define LO_NH_ICMP 0xf8


// cd: base class for MAC layer
// function pointer implement by child class

typedef struct {
    list_node_t node;
    uint8_t     dat[256 + 3 + 2];
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
    void    (* set_mac_filter)(struct cd_intf *intf, uint8_t filter);
    uint8_t (* get_mac_filter)(struct cd_intf *intf);

    void    (* flush)(struct cd_intf *intf);
} cd_intf_t;


// lo: short for 6lo

typedef struct {
    list_node_t node;

    uint8_t     src_addr_type;
    uint8_t     src_addr[16]; // not care unused bytes
    uint8_t     src_mac;
    uint8_t     dst_addr_type;
    uint8_t     dst_addr[16];
    uint8_t     dst_mac;

    uint8_t     pkt_type;     // not include P and RSV

    uint16_t    src_udp_port;
    uint16_t    dst_udp_port;

    uint8_t     icmp_type;

    uint8_t     dat_len;
    uint8_t     dat[249];
} lo_packet_t;

typedef struct {
    uint8_t     mac;
    uint8_t     site_id;      // use 1 byte at now

    list_head_t *free_head;
    list_head_t rx_head;
    list_head_t tx_head;

    cd_intf_t   *cd_intf;
} lo_intf_t;


void lo_intf_init(lo_intf_t *intf, list_head_t *free_head,
    cd_intf_t *cd_intf, uint8_t mac);


// helper

lo_packet_t *lo_get_udp_packet(lo_intf_t *intf, uint16_t dst_port);
void lo_exchange_src_dst(lo_intf_t *intf, lo_packet_t *pkt);
void lo_fill_src_addr(lo_intf_t *intf, lo_packet_t *pkt);

void lo_rx(lo_intf_t *intf);
void lo_tx(lo_intf_t *intf);

#endif

