/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdnet.h"

#if __BYTE_ORDER != __LITTLE_ENDIAN
# error "Please fix endianness of multicast_id and ports"
#endif

#define assert(expr) { if (!(expr)) return ERR_ASSERT; }


static int get_port_size(uint8_t val, uint8_t *src_size, uint8_t *dst_size)
{
    switch (val) {
    case 0x00: *src_size = 0; *dst_size = 1; break;
    case 0x01: *src_size = 0; *dst_size = 2; break;
    case 0x02: *src_size = 1; *dst_size = 0; break;
    case 0x03: *src_size = 2; *dst_size = 0; break;
    case 0x04: *src_size = 1; *dst_size = 1; break;
    case 0x05: *src_size = 1; *dst_size = 2; break;
    case 0x06: *src_size = 2; *dst_size = 1; break;
    case 0x07: *src_size = 2; *dst_size = 2; break;
    default: return -1;
    }
    return 0;
}

static int cal_port_val(uint16_t src, uint16_t dst,
        uint8_t *src_size, uint8_t *dst_size)
{
    if (src == CDNET_DEF_PORT)
        *src_size = 0;
    else if (src <= 0xff)
        *src_size = 1;
    else
        *src_size = 2;

    if (dst == CDNET_DEF_PORT)
        *dst_size = 0;
    else if (dst <= 0xff)
        *dst_size = 1;
    else
        *dst_size = 2;

    switch ((*src_size << 4) | *dst_size) {
    case 0x01: return 0x00;
    case 0x02: return 0x01;
    case 0x10: return 0x02;
    case 0x20: return 0x03;
    case 0x11: return 0x04;
    case 0x12: return 0x05;
    case 0x21: return 0x06;
    case 0x22: return 0x07;
    default: return -1;
    }
}


int cdnet_l1_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf)
{
    int ret;
    uint8_t src_port_size;
    uint8_t dst_port_size;

    uint8_t *buf_s = buf;
    uint8_t *hdr = buf + 3;

    assert(pkt->level == CDNET_L1);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end
    *buf++ = HDR_L1_L2 | (pkt->multi << 4); // hdr

    switch (pkt->multi) {
    case CDNET_MULTI_CAST_NET:
    case CDNET_MULTI_NET:
        *buf++ = pkt->src_addr.net;
        *buf++ = pkt->src_addr.mac;
        // no break
    case CDNET_MULTI_CAST:
        // multicast_id also copied
        *buf++ = pkt->dst_addr.net;
        *buf++ = pkt->dst_addr.mac;
    default:
        break;
    }

    if (pkt->seq) {
        *hdr |= HDR_L1_SEQ;
        *buf++ = pkt->_seq_num | (pkt->_req_ack << 7);
    }

    ret = cal_port_val(pkt->src_port, pkt->dst_port,
            &src_port_size, &dst_port_size);
    *hdr |= ret;

    if (src_port_size >= 1)
        *buf++ = pkt->src_port & 0xff;
    if (src_port_size == 2)
        *buf++ = pkt->src_port >> 8;
    if (dst_port_size >= 1)
        *buf++ = pkt->dst_port & 0xff;
    if (dst_port_size == 2)
        *buf++ = pkt->dst_port >> 8;

    assert(buf - buf_s + pkt->len <= 256);
    *(buf_s + 2) = buf - buf_s + pkt->len - 3;
    memcpy(buf, pkt->dat, pkt->len);
    return 0;
}

int cdnet_l1_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    uint8_t src_port_size;
    uint8_t dst_port_size;

    const uint8_t *buf_s = buf;
    const uint8_t *hdr = buf + 3;
    uint8_t tmp_len;

    assert((*hdr & 0xc0) == 0x80);
    pkt->level = CDNET_L1;

    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    tmp_len = *buf++;
    assert(tmp_len >= 1);
    pkt->len = 0;
    buf++; // skip hdr

    pkt->seq = !!(*hdr & HDR_L1_SEQ);
    pkt->multi = (*hdr >> 4) & 3;

    switch (pkt->multi) {
    case CDNET_MULTI_CAST_NET:
    case CDNET_MULTI_NET:
        pkt->src_addr.net = *buf++;
        pkt->src_addr.mac = *buf++;
        // no break
    case CDNET_MULTI_CAST:
        // multicast_id also copied
        pkt->dst_addr.net = *buf++;
        pkt->dst_addr.mac = *buf++;
    default:
        break;
    }

    if (pkt->seq) {
        pkt->_seq_num = *buf++;
        pkt->_req_ack = !!(pkt->_seq_num & 0x80);
        pkt->_seq_num &= 0x7f;
    }

    get_port_size(*hdr & 0x07, &src_port_size, &dst_port_size);

    if (src_port_size == 0) {
        pkt->src_port = CDNET_DEF_PORT;
    } else {
        if (src_port_size >= 1)
            pkt->src_port = *buf++;
        if (src_port_size == 2)
            pkt->src_port |= *buf++ << 8;
    }
    if (dst_port_size == 0) {
        pkt->dst_port = CDNET_DEF_PORT;
    } else {
        if (dst_port_size >= 1)
            pkt->dst_port = *buf++;
        if (dst_port_size == 2)
            pkt->dst_port |= *buf++ << 8;
    }

    pkt->len = tmp_len - (buf - buf_s - 3);
    assert(pkt->len >= 0);
    memcpy(pkt->dat, buf, pkt->len);
    return 0;
}
