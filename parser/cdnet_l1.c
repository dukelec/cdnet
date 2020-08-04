/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdnet.h"


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

    if (dst == CDNET_DEF_PORT && src != CDNET_DEF_PORT)
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


int cdnet_l1_to_frame(const cd_sockaddr_t *src, const cd_sockaddr_t *dst,
        const uint8_t *dat, uint8_t len, uint8_t src_mac,
        uint8_t seq_val, uint8_t *frame)
{
    int ret;
    uint8_t src_port_size;
    uint8_t dst_port_size;

    uint8_t *buf = frame;
    uint8_t *hdr = frame + 3;
    cdnet_multi_t multi = (dst->addr[0] >> 4) & 3;

    assert((dst->addr[0] & 0xc0) == 0x80 || (dst->addr[0] & 7) == 0);

    *buf++ = src_mac;
    buf += 3; // skip hdr
    *hdr = HDR_L1_L2 | (multi << 4); // hdr

    switch (multi) {
    case CDNET_MULTI_CAST:
        // multicast_id
        *buf++ = dst->addr[1];
        *buf++ = dst->addr[2];
        assert(frame[0] == src->addr[2])
        frame[1] = dst->addr[2];
        break;
    case CDNET_MULTI_NET:
        *buf++ = src->addr[1];
        *buf++ = src->addr[2];
        *buf++ = dst->addr[1];
        *buf++ = dst->addr[2];
        //frame[1] = cdnet_get_router(&dst->addr);
        assert(false); // not support multi-net currently
        break;
    case CDNET_MULTI_CAST_NET:
        *buf++ = src->addr[1];
        *buf++ = src->addr[2];
        // multicast_id
        *buf++ = dst->addr[1];
        *buf++ = dst->addr[2];
        // TODO: set to 255 if remote hw filter not enough
        frame[1] = dst->addr[2];
        break;
    default:
        assert(frame[0] == src->addr[2])
        frame[1] = dst->addr[2];
        break;
    }

    if (dst->addr[0] & 8) {
        *hdr |= HDR_L1_L2_SEQ;
        *buf++ = seq_val;
    }

    ret = cal_port_val(src->port, dst->port,
            &src_port_size, &dst_port_size);
    *hdr |= ret;

    if (src_port_size >= 1)
        *buf++ = src->port & 0xff;
    if (src_port_size == 2)
        *buf++ = src->port >> 8;
    if (dst_port_size >= 1)
        *buf++ = dst->port & 0xff;
    if (dst_port_size == 2)
        *buf++ = dst->port >> 8;

    assert(buf - frame + len <= 256);
    *(frame + 2) = buf - frame + len - 3;
    memcpy(buf, dat, len);
    return 0;
}

int cdnet_l1_from_frame(const uint8_t *frame, uint8_t local_net,
        cd_sockaddr_t *src, cd_sockaddr_t *dst, uint8_t *dat, uint8_t *len,
        uint8_t *seq_val)
{
    uint8_t src_port_size;
    uint8_t dst_port_size;

    const uint8_t *buf = frame;
    const uint8_t *hdr = frame + 3;

    assert((*hdr & 0xc0) == 0x80);

    uint8_t src_mac = *buf++;
    uint8_t dst_mac = *buf++;
    assert(frame[2] >= 1);
    buf += 2; // skip hdr

    bool seq = !!(*hdr & HDR_L1_L2_SEQ);
    cdnet_multi_t multi = (*hdr >> 4) & 3;

    switch (multi) {
    case CDNET_MULTI_CAST:
        src->addr[0] = seq ? 0x88 : 0x80;
        src->addr[1] = local_net;
        src->addr[2] = src_mac;
        dst->addr[0] = seq ? 0x98 : 0x90;
        dst->addr[1] = *buf++;
        dst->addr[2] = *buf++;
        break;
    case CDNET_MULTI_NET:
        src->addr[0] = seq ? 0xa8 : 0xa0;
        src->addr[1] = *buf++;
        src->addr[2] = *buf++;
        dst->addr[0] = seq ? 0xa8 : 0xa0;
        dst->addr[1] = *buf++;
        dst->addr[2] = *buf++;
        break;
    case CDNET_MULTI_CAST_NET:
        src->addr[0] = seq ? 0xa8 : 0xa0;
        src->addr[1] = *buf++;
        src->addr[2] = *buf++;
        dst->addr[0] = seq ? 0xb8 : 0xb0;
        dst->addr[1] = *buf++;
        dst->addr[2] = *buf++;
        break;
    default:
        src->addr[0] = seq ? 0x88 : 0x80;
        src->addr[1] = local_net;
        src->addr[2] = src_mac;
        dst->addr[0] = seq ? 0x88 : 0x80;
        dst->addr[1] = local_net;
        dst->addr[2] = dst_mac;
        break;
    }

    if (seq)
        *seq_val = *buf++;

    get_port_size(*hdr & 0x07, &src_port_size, &dst_port_size);

    if (src_port_size == 0) {
        src->port = CDNET_DEF_PORT;
    } else {
        if (src_port_size >= 1)
            src->port = *buf++;
        if (src_port_size == 2)
            src->port |= *buf++ << 8;
    }
    if (dst_port_size == 0) {
        dst->port = CDNET_DEF_PORT;
    } else {
        if (dst_port_size >= 1)
            dst->port = *buf++;
        if (dst_port_size == 2)
            dst->port |= *buf++ << 8;
    }

    *len = frame[2] - (buf - frame - 3);
    assert(*len >= 0);
    memcpy(dat, buf, *len);
    return 0;
}
