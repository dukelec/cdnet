/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet.h"


int cdn0_to_payload(const cdn_pkt_t *pkt, uint8_t *payload)
{
    const cdn_sockaddr_t *src = &pkt->src;
    const cdn_sockaddr_t *dst = &pkt->dst;
    const uint8_t *dat = pkt->dat;
    uint8_t len = pkt->len;

    cdn_assert(dst->addr[0] == 0x00);
    cdn_assert((src->port == CDN_DEF_PORT && dst->port <= 63) || dst->port == CDN_DEF_PORT);

    if (src->port == CDN_DEF_PORT) { // out request
#ifndef CDN_L0_C
        cdn_assert(false); // not support
#endif
        *payload = dst->port; // hdr
    } else { // out reply
        cdn_assert(len >= 1);
        cdn_assert((dat[0] & 0xc0) == CDN0_SHARE_LEFT);
        *payload = CDN_HDR_L0_REPLY | (dat[0] & 0x3f); // share first byte
        len--;
        dat++;
    }

    cdn_assert(len + 1 <= 253);
    memcpy(payload + 1, dat, len);
    return len + 1;
}

int cdn0_to_frame(const cdn_pkt_t *pkt, uint8_t *frame)
{
    frame[0] = pkt->src.addr[2];
    frame[1] = pkt->dst.addr[2];
    int ret = cdn0_to_payload(pkt, frame + 3);
    if (ret < 0)
        return ret;
    frame[2] = ret;
    return 0;
}


int cdn0_from_payload(const uint8_t *payload, uint8_t len, cdn_pkt_t *pkt)
{
    cdn_sockaddr_t *src = &pkt->src;
    cdn_sockaddr_t *dst = &pkt->dst;
    uint8_t *dat = pkt->dat;

    cdn_assert(!(*payload & 0x80));
    src->addr[0] = 0;
    src->addr[1] = pkt->_l_net;
    dst->addr[0] = 0;
    dst->addr[1] = pkt->_l_net;
    src->addr[2] = pkt->_s_mac;
    dst->addr[2] = pkt->_d_mac;
    pkt->len = len - 1;

    if (*payload & CDN_HDR_L0_REPLY) { // in reply
#ifdef CDN_L0_C
        src->port = pkt->_l0_lp;
        dst->port = CDN_DEF_PORT;
        pkt->len++;
        *dat++ = (*payload & 0x3f) | CDN0_SHARE_LEFT;
#else
        cdn_assert(false); // not support
#endif
    } else { // in request
        src->port = CDN_DEF_PORT;
        dst->port = *payload;
    }

    memcpy(dat, payload + 1, len - 1);
    return 0;
}

// addition in: _l_net, _l0_lp (central only)
int cdn0_from_frame(const uint8_t *frame, cdn_pkt_t *pkt)
{
    pkt->_s_mac = frame[0];
    pkt->_d_mac = frame[1];
    return cdn0_from_payload(frame + 3, frame[2], pkt);
}
