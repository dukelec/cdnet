/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDNET_H__
#define __CDNET_H__

#include "cd_utils.h"
#include "arch_wrapper.h"
#include "cd_list.h"

#ifndef CDNET_DEF_PORT
#define CDNET_DEF_PORT      0xcdcd
#endif
#ifndef CDNET_LOCAL_PORT    // ephemeral port start
#define CDNET_LOCAL_PORT    32768
#endif

#ifndef CDNET_DAT_SIZE
#define CDNET_DAT_SIZE      252
#endif

#ifndef SEQ_RX_REC_MAX
#define SEQ_RX_REC_MAX      3
#endif
#ifndef SEQ_TX_REC_MAX
#define SEQ_TX_REC_MAX      3
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

#ifdef CDNET_IRQ_SAFE
#define cdnet_packet_get(head)  list_get_entry_it(head, cdnet_packet_t)
#define cdnet_list_put          list_put_it
#define cdnet_list_put_begin    list_put_begin_it
#elif !defined(CDNET_USER_LIST)
#define cdnet_packet_get(head)  list_get_entry(head, cdnet_packet_t)
#define cdnet_list_put          list_put
#define cdnet_list_put_begin    list_put_begin
#endif


typedef enum {
    CDNET_L0 = 0,
    CDNET_L1,
    CDNET_L2
} cdnet_level_t;

typedef enum {
    CDNET_MULTI_NONE = 0,
    CDNET_MULTI_CAST,
    CDNET_MULTI_NET,
    CDNET_MULTI_CAST_NET

} cdnet_multi_t;

typedef enum {
    CDNET_FRAG_NONE = 0,
    CDNET_FRAG_FIRST,
    CDNET_FRAG_MORE,
    CDNET_FRAG_LAST
} cdnet_frag_t;

#define HDR_L1_L2       (1 << 7)
#define HDR_L2          (1 << 6)

#define HDR_L0_REPLY    (1 << 6)
#define HDR_L0_SHARE    (1 << 5)

#define HDR_L1_L2_SEQ   (1 << 3)

#define ERR_ASSERT      -1


// cd: base class for MAC layer
// function pointer implement by child class

typedef struct {
    list_node_t node;
    uint8_t     dat[260]; // max size for cdbus through uart
} cd_frame_t;

typedef struct cd_intf {

    cd_frame_t *(* get_free_frame)(struct cd_intf *cd_intf);
    cd_frame_t *(* get_rx_frame)(struct cd_intf *cd_intf);
    void (* put_free_frame)(struct cd_intf *cd_intf, cd_frame_t *frame);
    void (* put_tx_frame)(struct cd_intf *cd_intf, cd_frame_t *frame);

    // cdbus has two baud rates
    void  (* set_baud_rate)(struct cd_intf *intf, uint32_t, uint32_t);
    void  (* get_baud_rate)(struct cd_intf *intf, uint32_t *, uint32_t *);

    // 255 for promiscuous mode
    void    (* set_filter)(struct cd_intf *intf, uint8_t filter);
    uint8_t (* get_filter)(struct cd_intf *intf);

    void    (* set_tx_wait)(struct cd_intf *intf, uint8_t len);
    uint8_t (* get_tx_wait)(struct cd_intf *intf);

    void    (* flush)(struct cd_intf *intf);
} cd_intf_t;


typedef struct{
    uint8_t     net; // net id
    uint8_t     mac; // mac id
} cdnet_addr_t;

typedef struct {
    list_node_t     node;

    cdnet_level_t   level;

    bool            seq; // enable sequence
    // set by cdnet_tx:
    bool            _req_ack;
    uint8_t         _seq_num;
    uint32_t        _send_time;

    cdnet_multi_t   multi;

    // local send and receive addresses
    uint8_t         src_mac;
    uint8_t         dst_mac;

    // original send and receive addresses
    cdnet_addr_t    src_addr;
    union {
        cdnet_addr_t    __dst_addr;
        uint16_t        __multicast_id;
    } __dst_u;
#define dst_addr        __dst_u.__dst_addr
#define multicast_id    __dst_u.__multicast_id

    uint16_t        src_port;
    uint16_t        dst_port;

    // level 2 only
    cdnet_frag_t    frag;
    uint8_t         l2_flag;

    int             len;
    uint8_t         dat[CDNET_DAT_SIZE];
} cdnet_packet_t;


typedef struct {
    list_node_t     node;
    cdnet_addr_t    addr; // net = 255: link local; mac = 255: not used
    uint8_t         seq_num;
} seq_rx_rec_t;

typedef struct {
    list_node_t     node;
    cdnet_addr_t    addr; // net = 255: link local; mac = 255: not used
    uint8_t         seq_num;

    // for tx only
    list_head_t     wait_head;
    list_head_t     pend_head;
    uint8_t         send_cnt; // send ack for each SEQ_TX_ACK_CNT
    uint8_t         p0_retry_cnt;
    cdnet_packet_t  *p0_req;
} seq_tx_rec_t;

typedef struct {
    const char      *name;
    cdnet_addr_t    addr; // interface address
    uint8_t         l0_last_port; // don't override before receive the reply

    list_head_t     *free_head;
    list_head_t     rx_head;
    list_head_t     tx_head;

    cd_intf_t       *cd_intf;

    seq_rx_rec_t    seq_rx_rec_alloc[SEQ_RX_REC_MAX];
    seq_tx_rec_t    seq_tx_rec_alloc[SEQ_TX_REC_MAX];
    list_head_t     seq_rx_head;
    list_head_t     seq_tx_head;
    list_head_t     seq_tx_direct_head;
} cdnet_intf_t;


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
    cd_intf_t *cd_intf, cdnet_addr_t *addr);


// helper

void cdnet_exchg_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt);

void cdnet_rx(cdnet_intf_t *intf);
void cdnet_tx(cdnet_intf_t *intf);

static inline bool is_addr_equal(const cdnet_addr_t *a, const cdnet_addr_t *b)
{
    return a->mac == b->mac && a->net == b->net;
}

#endif
