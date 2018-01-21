/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */


#include "cdnet.h"

#define assert(expr) { if (!(expr)) return ERR_ASSERT; }


int cdnet_l0_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf)
{
    uint8_t *buf_s = buf;

    assert(pkt->level == CDNET_L0);
    assert((pkt->src_port == CDNET_DEF_PORT && pkt->dst_port <= 63) ||
            pkt->dst_port == CDNET_DEF_PORT);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end

    if (pkt->src_port == CDNET_DEF_PORT) { // out request
        intf->l0_last_port = pkt->dst_port;
        *buf++ = pkt->dst_port; // hdr
    } else { // out reply
        if (pkt->len >= 1 && pkt->dat[0] <= 31) {
            // share first byte
            pkt->dat[0] |= HDR_L0_REPLY | HDR_L0_SHARE;
        } else {
            *buf++ = HDR_L0_REPLY; // hdr
        }
    }

    assert(buf - buf_s + pkt->len <= 256);
    *(buf_s + 2) = buf - buf_s + pkt->len - 3;
    memcpy(buf, pkt->dat, pkt->len);
    return 0;
}

int cdnet_l0_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    const uint8_t *hdr = buf + 3;
    uint8_t *cpy_to = pkt->dat;
    uint8_t cpy_len;
    uint8_t tmp_len;

    assert(!(*hdr & 0x80));
    pkt->level = CDNET_L0;

    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    tmp_len = *buf++;
    assert(tmp_len >= 1);
    buf++; // skip hdr

    pkt->len = tmp_len - 1;
    cpy_len = pkt->len;

    if (*hdr & HDR_L0_REPLY) { // in reply
        pkt->src_port = intf->l0_last_port;
        pkt->dst_port = CDNET_DEF_PORT;
        if (*hdr & HDR_L0_SHARE) {
            pkt->len = tmp_len;
            cpy_len = pkt->len - 1;
            *cpy_to++ = *hdr & 0x1f;
        }
    } else { // in request
        pkt->src_port = CDNET_DEF_PORT;
        pkt->dst_port = *hdr;
    }

    assert(pkt->len >= 0);
    memcpy(cpy_to, buf, cpy_len);
    return 0;
}
