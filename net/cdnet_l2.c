/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

/*
 * The CDBUS Level 2 implementation
 * Limitation: must select SEQ_NO
 */

#include "cdnet.h"

#define assert(expr) { if (!(expr)) return ERR_ASSERT; }


int cdnet_l2_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf)
{
    uint8_t *buf_s = buf;
    uint8_t *hdr = buf + 3;

    assert(pkt->level == CDNET_L2);
    assert(pkt->is_seq);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end

    *buf++ = HDR_L1_L2 | HDR_L2 | HDR_L2_SEQ_NUM; // hdr

    if (pkt->is_fragment) {
        *hdr |= HDR_L2_FRAGMENT;
        if (pkt->is_fragment_end)
            *hdr |= HDR_L2_FRAGMENT_END;
    }

    if (pkt->is_compressed)
        *hdr |= HDR_L2_COMPRESSED;

    *buf++ = pkt->seq_num | (pkt->req_ack << 7);

    assert(buf - buf_s + pkt->dat_len <= 256);
    *(buf_s + 2) = buf - buf_s + pkt->dat_len - 3;
    memcpy(buf, pkt->dat, pkt->dat_len);
    return 0;
}

int cdnet_l2_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    const uint8_t *hdr = buf + 3;
    uint8_t tmp_len;

    assert((*hdr & 0xc0) == 0xc0);
    assert(*hdr & HDR_L1_SEQ_NUM);
    pkt->level = CDNET_L2;
    pkt->is_seq = true;

    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    tmp_len = *buf++;

    buf++; // skip hdr

    if (*hdr & HDR_L2_FRAGMENT) {
        pkt->is_fragment = true;
        pkt->is_fragment_end = !!(*hdr & HDR_L2_FRAGMENT_END);
    } else {
        pkt->is_fragment = false;
    }

    pkt->is_compressed = !!(*hdr & HDR_L2_COMPRESSED);

    pkt->seq_num = *buf++;
    pkt->req_ack = !!(pkt->seq_num & 0x80);
    pkt->seq_num &= 0x7f;

    pkt->dat_len = tmp_len - 2;
    assert(pkt->dat_len >= 0);
    memcpy(pkt->dat, buf, pkt->dat_len);
    return 0;
}
