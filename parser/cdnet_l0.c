/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdbus.h"
#include "cdnet.h"


int cdn0_hdr_w(const cdn_pkt_t *pkt, uint8_t *hdr)
{
    const cdn_sockaddr_t *src = &pkt->src;
    const cdn_sockaddr_t *dst = &pkt->dst;
    const uint8_t *dat = pkt->dat;

    cdn_assert(dst->addr[0] == 0x00);
    cdn_assert((src->port == CDN_DEF_PORT && dst->port <= 63) || dst->port == CDN_DEF_PORT);

    if (src->port == CDN_DEF_PORT) {    // out request
#ifndef CDN_L0_C
        cdn_assert(false);              // not support
#endif
        *hdr = dst->port;               // hdr
        return 1;
    } else {                            // out reply
        cdn_assert(pkt->len >= 1);
        cdn_assert((dat[0] & 0xc0) == CDN0_SHARE_LEFT);
        *hdr = CDN_HDR_L0_REPLY | (dat[0] & 0x3f); // share first byte
        return 0;
    }
}

int cdn0_frame_w(cdn_pkt_t *pkt)
{
    uint8_t *frame = pkt->frm->dat;
    frame[0] = pkt->src.addr[2];
    frame[1] = pkt->dst.addr[2];
    int len = cdn0_hdr_w(pkt, frame + 3);
    if (len < 0)
        return len;
    frame[2] = pkt->len + len;
    return 0;
}


int cdn0_hdr_r(cdn_pkt_t *pkt, const uint8_t *hdr)
{
    cdn_sockaddr_t *src = &pkt->src;
    cdn_sockaddr_t *dst = &pkt->dst;

    cdn_assert(!(*hdr & 0x80));
    src->addr[0] = 0;
    src->addr[1] = pkt->_l_net;
    dst->addr[0] = 0;
    dst->addr[1] = pkt->_l_net;
    src->addr[2] = pkt->_s_mac;
    dst->addr[2] = pkt->_d_mac;

    if (*hdr & CDN_HDR_L0_REPLY) {  // in reply
#ifdef CDN_L0_C
        src->port = pkt->_l0_lp;
        dst->port = CDN_DEF_PORT;
#else
        cdn_assert(false);          // not support
#endif
        return 0;
    } else {                        // in request
        src->port = CDN_DEF_PORT;
        dst->port = *hdr;
        return 1;
    }
}

// addition in: _l_net, _l0_lp (central only)
int cdn0_frame_r(cdn_pkt_t *pkt)
{
    uint8_t *frame = pkt->frm->dat;
    pkt->_s_mac = frame[0];
    pkt->_d_mac = frame[1];
    int len = cdn0_hdr_r(pkt, frame + 3);
    if (len < 0)
        return len;
    pkt->dat = frame + 3 + len;
    pkt->len = frame[2] - len;
    if (len == 0)
        frame[3] = (frame[3] & 0x3f) | CDN0_SHARE_LEFT;
    return 0;
}
