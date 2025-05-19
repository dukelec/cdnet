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

typedef enum {
    CDN_MULTI_NONE = 0,
    CDN_MULTI_CAST,
    CDN_MULTI_NET,
    CDN_MULTI_CAST_NET
} cdn_multi_t;


typedef struct {
    uint8_t  addr[3]; // [type, net, mac] or [type, mh, ml]
    uint16_t port;
} cdn_sockaddr_t;


#ifndef CDN_MAX_DAT
#define CDN_MAX_DAT         252      // allow smaller sizes to save memory
#endif

#define CDN_CONF_NOT_FREE   (1 << 0) // not free packet after transmit

#define CDN_RET_FMT_ERR     (1 << 1)
#define CDN_RET_ROUTE_ERR   (1 << 2)

typedef struct {
    list_node_t     node;
    uint8_t         _s_mac;
    uint8_t         _d_mac;
    uint8_t         _l_net; // local net

    uint8_t         conf;
    uint8_t         ret;    // bit7: 0: init, 1: finished (bit6~0: error code)

    cdn_sockaddr_t  src;
    cdn_sockaddr_t  dst;

    cd_frame_t      *frm;
    uint8_t         *dat;
    uint8_t         len;
} cdn_pkt_t;

#ifdef CDN_IRQ_SAFE
#define cdn_list_get(head)               list_get_entry_it(head, cdn_pkt_t)
#define cdn_list_put(head, frm)          list_put_it(head, &(frm)->node)
#elif !defined(CDN_USER_LIST)
#define cdn_list_get(head)               list_get_entry(head, cdn_pkt_t)
#define cdn_list_put(head, frm)          list_put(head, &(frm)->node)
#endif

int cdn_hdr_size_pkt(const cdn_pkt_t *pkt);
int cdn_hdr_size_frm(const cd_frame_t *frm);

int cdn0_hdr_w(const cdn_pkt_t *pkt, uint8_t *hdr);
int cdn0_hdr_r(cdn_pkt_t *pkt, const uint8_t *hdr);
int cdn1_hdr_w(const cdn_pkt_t *pkt, uint8_t *hdr);
int cdn1_hdr_r(cdn_pkt_t *pkt, const uint8_t *hdr);

int cdn0_frame_w(cdn_pkt_t *pkt);
int cdn0_frame_r(cdn_pkt_t *pkt);
int cdn1_frame_w(cdn_pkt_t *pkt);
int cdn1_frame_r(cdn_pkt_t *pkt);

static inline void cdn_set_addr(uint8_t *addr, uint8_t a0, uint8_t a1, uint8_t a2)
{
    addr[0] = a0;
    addr[1] = a1;
    addr[2] = a2;
}

static inline int cdn_frame_w(cdn_pkt_t *pkt)
{
    if (pkt->src.addr[0] == 0x10) // localhost
        return -1;
    if (pkt->src.addr[0] == 0x00)
        return cdn0_frame_w(pkt);
    return cdn1_frame_w(pkt);
}

static inline int cdn_frame_r(cdn_pkt_t *pkt)
{
    if (pkt->frm->dat[3] & 0x80)
        return cdn1_frame_r(pkt);
    return cdn0_frame_r(pkt);
}

#endif
