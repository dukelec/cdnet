/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

/* CDNET address string format:
 *
 *              local link     unique local    local and cross net multicast
 * level0:       00:NN:MM
 * level1:       80:NN:MM        a0:NN:MM     90:M_ID     b0:M_ID
 *  `-with seq:  88:NN:MM        a8:NN:MM     98:M_ID     b8:M_ID
 * level2:       c0:NN:MM
 *  `-with seq:  c8:NN:MM
 *
 * Notes:
 *   NN: net_id, MM: mac_addr, M_ID: multicast_id (include scope)
 */

#ifndef __CDNET_H__
#define __CDNET_H__

#include "cd_utils.h"


#ifndef assert
#define ERR_ASSERT      -1
#define assert(expr) { if (!(expr)) return ERR_ASSERT; }
#endif

#if __BYTE_ORDER != __LITTLE_ENDIAN
#error "This library only support little endian at now"
#endif


#ifndef CDNET_DEF_PORT
#define CDNET_DEF_PORT  0xcdcd
#endif

#ifndef L0_SHARE_MASK
#define L0_SHARE_MASK   0xe0
#endif
#ifndef L0_SHARE_LEFT
#define L0_SHARE_LEFT   0x80
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


typedef struct {
    union {
        uint32_t    addr;
        uint8_t     addr8[3];
        struct {
            uint8_t type;
            uint8_t net;
            uint8_t mac;
        } cd_field_s;
        struct {
            uint8_t type;
            uint16_t m_id;
        } cd_mcast_s;
    } cd_addr_u;
#define cd_addr         cd_addr_u.addr
#define cd_addr8        cd_addr_u.addr8
#define cd_addr_type    cd_addr_u.cd_field_s.type
#define cd_addr_net     cd_addr_u.cd_field_s.net
#define cd_addr_mac     cd_addr_u.cd_field_s.mac
#define cd_addr_m_id    cd_addr_u.cd_mcast_s.m_id
} cd_addr_t;

typedef struct {
    cd_addr_t addr;
    uint16_t port;
} cd_sockaddr_t;


static inline void cdnet_set_addr(cd_addr_t *addr,
        uint8_t type, uint8_t net, uint8_t mac)
{
    addr->cd_addr_type = type;
    addr->cd_addr_net = net;
    addr->cd_addr_mac = mac;
}

static inline void cdnet_set_addr_mcast(cd_addr_t *addr,
        uint8_t type, uint16_t m_id)
{
    addr->cd_addr_type = type;
    addr->cd_addr_m_id = m_id;
}

static inline void cdnet_set_sockaddr(cd_sockaddr_t *addr,
        uint8_t type, uint8_t net, uint8_t mac, uint16_t port)
{
    cdnet_set_addr(&addr->addr, type, net, mac);
    addr->port = port;
}

static inline void cdnet_set_sockaddr_mcast(cd_sockaddr_t *addr,
        uint8_t type, uint16_t m_id, uint16_t port)
{
    cdnet_set_addr_mcast(&addr->addr, type, m_id);
    addr->port = port;
}


int cdnet_l0_to_frame(const cd_sockaddr_t *src, const cd_sockaddr_t *dst,
        const uint8_t *dat, uint8_t len, bool allow_share, uint8_t *frame);

int cdnet_l0_from_frame(const uint8_t *frame,
        uint8_t local_net, uint16_t last_port,
        cd_sockaddr_t *src, cd_sockaddr_t *dst, uint8_t *dat, uint8_t *len);

int cdnet_l1_to_frame(const cd_sockaddr_t *src, const cd_sockaddr_t *dst,
        const uint8_t *dat, uint8_t len, uint8_t src_mac,
        uint8_t seq_val, uint8_t *frame);

int cdnet_l1_from_frame(const uint8_t *frame, uint8_t local_net,
        cd_sockaddr_t *src, cd_sockaddr_t *dst, uint8_t *dat, uint8_t *len,
        uint8_t *seq_val);

int cdnet_l2_to_frame(const cd_addr_t *s_addr, const cd_addr_t *d_addr,
        const uint8_t *dat, uint32_t len, uint8_t user_flag,
        uint8_t max_size, uint8_t seq_val, uint32_t pos, uint8_t *frame);

int cdnet_l2_from_frame(const uint8_t *frame, uint8_t local_net,
        cd_addr_t *s_addr, cd_addr_t *d_addr, uint8_t *dat, uint8_t *len,
        uint8_t *user_flag, uint8_t *seq_val, cdnet_frag_t *frag);

#endif
