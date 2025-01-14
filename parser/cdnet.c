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


int cdn_hdr_size_pkt(const cdn_pkt_t *pkt)
{
    int hdr_size = 1;
    
    if (pkt->src.addr[0] == 0x10)   // localhost
        return 0;
    
    if (pkt->src.addr[0] == 0x00) {
        if (pkt->src.port == CDN_DEF_PORT)
            return 1;               // level 0 request
        return 0;                   // level 0 reply
    }

    if (pkt->src.addr[0] == 0xa0)
        hdr_size += 4;
    else if (pkt->dst.addr[0] == 0xf0)
        hdr_size += 2;

    if (pkt->src.port != CDN_DEF_PORT)
        hdr_size += pkt->src.port <= 0xff ? 1 : 2;

    if (pkt->dst.port != CDN_DEF_PORT || pkt->src.port == CDN_DEF_PORT)
        hdr_size += pkt->dst.port <= 0xff ? 1 : 2;

    return hdr_size;
}

int cdn_hdr_size_frm(const cd_frame_t *frm)
{
    uint8_t hdr = frm->dat[3];
    int hdr_size = 1;
    
    if ((hdr & 0xc0) == 0)          // level 0 request
    if ((hdr & 0xc0) == 0x40)
        return 0;                   // level 0 reply

    if (hdr & 0x20)
        hdr_size += 4;
    else if (hdr & 0x10)
        hdr_size += 2;

    if ((hdr & 6) == 6)
        hdr_size += 4;
    else if ((hdr & 6) == 4)
        hdr_size += 2;
    else
        hdr_size++;

    return hdr_size;
}
