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


int cdn1_hdr_w(const cdn_pkt_t *pkt, uint8_t *hdr)
{
    const cdn_sockaddr_t *src = &pkt->src;
    const cdn_sockaddr_t *dst = &pkt->dst;
    uint8_t *buf = hdr + 1;

    // the dst type byte is the header byte with the PORT_SIZE bits cleared:
    // 80: local link, a0: unique local, 90: local multicast, b0: cross net multicast
    cdn_assert((dst->addr[0] & 0xcf) == 0x80);
    cdn_multi_t multi = (dst->addr[0] >> 4) & 3;
    *hdr = dst->addr[0]; // hdr

    if (multi & CDN_MULTI_NET) {
        *buf++ = src->addr[1];
        *buf++ = src->addr[2];
    }
    if (multi != CDN_MULTI_NONE) {
        *buf++ = dst->addr[1];
        *buf++ = dst->addr[2];
    }

    *buf++ = src->port & 0xff;
    if (src->port & 0xff00) {
        *buf++ = src->port >> 8;
        *hdr |= 2;
    }

    *buf++ = dst->port & 0xff;
    if (dst->port & 0xff00) {
        *buf++ = dst->port >> 8;
        *hdr |= 1;
    }

    return buf - hdr;
}

// addition in: _s_mac, _d_mac
int cdn1_frame_w(cdn_pkt_t *pkt)
{
    uint8_t *frame = pkt->frm->dat;
    frame[0] = pkt->_s_mac;
    frame[1] = pkt->_d_mac;
    int len = cdn1_hdr_w(pkt, frame + 3);
    if (len < 0)
        return len;
    frame[2] = pkt->len + len;
    return 0;
}


int cdn1_hdr_r(cdn_pkt_t *pkt, const uint8_t *hdr)
{
    cdn_sockaddr_t *src = &pkt->src;
    cdn_sockaddr_t *dst = &pkt->dst;

    const uint8_t *buf = hdr + 1;
    cdn_assert((*hdr & 0xc8) == 0x80);
    cdn_multi_t multi = (*hdr >> 4) & 3;

    if (multi & CDN_MULTI_NET) {
        src->addr[0] = 0xa0;
        src->addr[1] = *buf++;
        src->addr[2] = *buf++;
    } else {
        src->addr[0] = 0x80;
        src->addr[1] = pkt->_l_net;
        src->addr[2] = pkt->_s_mac;
    }
    dst->addr[0] = *hdr & 0xb0; // 80, 90, a0 or b0
    if (multi != CDN_MULTI_NONE) {
        dst->addr[1] = *buf++;
        dst->addr[2] = *buf++;
    } else {
        dst->addr[1] = pkt->_l_net;
        dst->addr[2] = pkt->_d_mac;
    }

    src->port = *buf++;
    if (*hdr & 2)
        src->port |= *buf++ << 8;

    dst->port = *buf++;
    if (*hdr & 1)
        dst->port |= *buf++ << 8;

    return buf - hdr;
}

// addition in: _l_net
int cdn1_frame_r(cdn_pkt_t *pkt)
{
    uint8_t *frame = pkt->frm->dat;
    pkt->_s_mac = frame[0];
    pkt->_d_mac = frame[1];
    int len = cdn1_hdr_r(pkt, frame + 3);
    if (len < 0)
        return len;
    cdn_assert(frame[2] >= len); // len byte must at least cover the header
    pkt->dat = frame + 3 + len;
    pkt->len = frame[2] - len;
    return 0;
}
