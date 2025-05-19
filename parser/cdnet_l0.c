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

    cdn_assert(dst->addr[0] == 0x00);
    cdn_assert((src->port & 0xff80) == 0);
    cdn_assert((dst->port & 0xff80) == 0);

    *hdr = src->port;
    *(hdr + 1) = dst->port;
    return 2;
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

    cdn_assert((*hdr & 0x80) == 0);
    src->addr[0] = 0;
    src->addr[1] = pkt->_l_net;
    dst->addr[0] = 0;
    dst->addr[1] = pkt->_l_net;
    src->addr[2] = pkt->_s_mac;
    dst->addr[2] = pkt->_d_mac;

    src->port = *hdr;
    dst->port = *(hdr + 1);
    cdn_assert((dst->port & 0x80) == 0);
    return 2;
}

// addition in: _l_net
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
    return 0;
}
