/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDNET_H__
#define __CDNET_H__

#include "cd_utils.h"
#include "cd_list.h"


#ifndef cdn_assert
#define CDN_ERR_ASSERT  -1
#define cdn_assert(expr) { if (!(expr)) return CDN_ERR_ASSERT; }
#endif

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error "This library currently only supports little-endian"
#endif

#ifndef CDN_DEF_PORT
#define CDN_DEF_PORT    0xcdcd
#endif

#ifndef CDN0_SHARE_MASK
#define CDN0_SHARE_MASK 0xe0
#endif
#ifndef CDN0_SHARE_LEFT
#define CDN0_SHARE_LEFT 0x80
#endif

typedef enum {
    CDN_L0 = 0,
    CDN_L1,
    CDN_L2
} cdn_level_t;

typedef enum {
    CDN_MULTI_NONE = 0,
    CDN_MULTI_CAST,
    CDN_MULTI_NET,
    CDN_MULTI_CAST_NET
} cdn_multi_t;

typedef enum {
    CDN_FRAG_NONE = 0,
    CDN_FRAG_FIRST,
    CDN_FRAG_MORE,
    CDN_FRAG_LAST
} cdn_frag_t;

#define CDN_HDR_L1L2        (1 << 7)
#define CDN_HDR_L2          (1 << 6)

#define CDN_HDR_L0_REPLY    (1 << 6)
#define CDN_HDR_L0_SHARE    (1 << 5)

#define CDN_HDR_L1L2_SEQ    (1 << 3)


typedef struct {
    uint8_t  addr[3]; // [type, net, mac] or [type, mh, ml]
    uint16_t port;
} cdn_sockaddr_t;


#ifndef CDN_MAX_DAT
#define CDN_MAX_DAT         252
#endif

#define CDN_CONF_NOT_FREE   (1 << 0) // not free packet after transmit
#define CDN_CONF_REQ_ACK    (1 << 1) // request ack

#define CDN_RET_NO_FREE     (1 << 0) // no free frame, pkt, or tgt
#define CDN_RET_FMT_ERR     (1 << 1)
#define CDN_RET_ROUTE_ERR   (1 << 2)

typedef struct {
    list_node_t     node;
    uint8_t         _s_mac;
    uint8_t         _d_mac;
    uint8_t         _l_net; // local net
    uint8_t         seq;    // seq value

#ifdef CDN_L0_C             // L0 role central
    uint8_t         _l0_lp; // last_port
#endif
#ifdef CDN_L2
    uint8_t         l2_uf;  // user flag
    cdn_frag_t      l2_frag;
#endif

    uint8_t         conf;
    uint8_t         ret;    // bit7: 0: init, 1: finished (bit6~0: error code)

    cdn_sockaddr_t  src;
    cdn_sockaddr_t  dst;

    uint8_t         len;
    uint8_t         dat[CDN_MAX_DAT];
} cdn_pkt_t;


int cdn0_to_payload(const cdn_pkt_t *pkt, uint8_t *payload);
int cdn0_from_payload(const uint8_t *payload, uint8_t len, cdn_pkt_t *pkt);
int cdn1_to_payload(const cdn_pkt_t *pkt, uint8_t *payload);
int cdn1_from_payload(const uint8_t *payload, uint8_t len, cdn_pkt_t *pkt);
int cdn2_to_payload(const cdn_pkt_t *pkt, uint8_t *payload);
int cdn2_from_payload(const uint8_t *payload, uint8_t len, cdn_pkt_t *pkt);

int cdn0_to_frame(const cdn_pkt_t *pkt, uint8_t *frame);
int cdn0_from_frame(const uint8_t *frame, cdn_pkt_t *pkt);
int cdn1_to_frame(const cdn_pkt_t *pkt, uint8_t *frame);
int cdn1_from_frame(const uint8_t *frame, cdn_pkt_t *pkt);
int cdn2_to_frame(const cdn_pkt_t *pkt, uint8_t *frame);
int cdn2_from_frame(const uint8_t *frame, cdn_pkt_t *pkt);

static inline void cdn_set_addr(uint8_t *addr, uint8_t a0, uint8_t a1, uint8_t a2)
{
    addr[0] = a0;
    addr[1] = a1;
    addr[2] = a2;
}

static inline void cdn_init_pkt(cdn_pkt_t *pkt)
{
    memset(pkt, 0, offsetof(cdn_pkt_t, dat));
}

#endif
