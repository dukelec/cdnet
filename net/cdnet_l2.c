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
    assert((pkt->l2_flag & ~7) == 0);
    assert((pkt->frag & ~3) == 0);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end
    *buf++ = HDR_L1_L2 | HDR_L2 | pkt->l2_flag; // hdr

    if (pkt->frag) {
        assert(pkt->seq);
        *hdr |= pkt->frag << 4;
    }

    if (pkt->seq) {
        *hdr |= HDR_L1_L2_SEQ;
        *buf++ = pkt->_seq_num | (pkt->_req_ack << 7);
    }

    assert(buf - buf_s + pkt->len <= 256);
    *(buf_s + 2) = buf - buf_s + pkt->len - 3;
    memcpy(buf, pkt->dat, pkt->len);
    return 0;
}

int cdnet_l2_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    const uint8_t *hdr = buf + 3;
    uint8_t tmp_len;

    assert((*hdr & 0xc0) == 0xc0);
    pkt->level = CDNET_L2;
    pkt->seq = !!(*hdr & HDR_L1_L2_SEQ);

    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    tmp_len = *buf++;
    pkt->l2_flag = *buf++ & 7; // hdr

    if (*hdr & 0x30) {
        assert(pkt->seq);
        pkt->frag = (*hdr >> 4) & 3;
    } else {
        pkt->frag = CDNET_FRAG_NONE;
    }

    if (pkt->seq) {
        pkt->_seq_num = *buf++;
        pkt->_req_ack = !!(pkt->_seq_num & 0x80);
        pkt->_seq_num &= 0x7f;
    }

    pkt->len = tmp_len - (pkt->seq ? 2 : 1);
    assert(pkt->len >= 0);
    memcpy(pkt->dat, buf, pkt->len);
    return 0;
}
