/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdnet.h"

/*
 * max_size <= 253, pos default 0
 * return body size on success
 */
int cdnet_l2_to_frame(const uint8_t *s_addr, const uint8_t *d_addr,
        const uint8_t *dat, uint32_t len, uint8_t user_flag,
        uint8_t max_size, uint8_t seq_val, uint32_t pos, uint8_t *frame)
{
    uint8_t *buf = frame;
    uint8_t *hdr = buf + 3;
    assert((user_flag & ~7) == 0);
    bool seq = !!(d_addr[0] & 8);
    uint8_t payload_max = seq ? max_size - 1 : max_size;
    cdnet_frag_t frag;
    assert(d_addr[0] == 0xc0 || d_addr[0] == 0xc8);

    *buf++ = s_addr[2];
    *buf++ = d_addr[2];
    *hdr = HDR_L1_L2 | HDR_L2 | user_flag;
    buf += 2; // skip hdr

    if (len - pos <= payload_max)
        frag = pos ? CDNET_FRAG_LAST : CDNET_FRAG_NONE;
    else
        frag = pos ? CDNET_FRAG_MORE : CDNET_FRAG_FIRST;

    if (seq) {
        *hdr |= HDR_L1_L2_SEQ;
        *buf++ = seq_val;
    }
    if (frag) {
        assert(seq);
        *hdr |= frag << 4;
    }

    dat += pos;
    len = min(len - pos, payload_max);

    assert(buf - frame + len <= 256);
    *(frame + 2) = buf - frame + len - 3;
    memcpy(buf, dat, len);
    return len;
}

int cdnet_l2_from_frame(const uint8_t *frame, uint8_t local_net,
        uint8_t *s_addr, uint8_t *d_addr, uint8_t *dat, uint8_t *len,
        uint8_t *user_flag, uint8_t *seq_val, cdnet_frag_t *frag)
{
    const uint8_t *buf = frame;
    const uint8_t *hdr = frame + 3;

    assert((*hdr & 0xc0) == 0xc0);
    bool seq = !!(*hdr & HDR_L1_L2_SEQ);
    *user_flag = *hdr & 7; // hdr

    s_addr[0] = seq ? 0xc8 : 0xc0;
    d_addr[0] = s_addr[0];
    s_addr[1] = local_net;
    d_addr[1] = local_net;
    s_addr[2] = *buf++;
    d_addr[2] = *buf++;
    buf += 2; // skip hdr

    if (*hdr & 0x30) {
        assert(seq);
        *frag = (*hdr >> 4) & 3;
    } else {
        *frag = CDNET_FRAG_NONE;
    }

    if (seq)
        *seq_val = *buf++;

    *len = frame[2] - (seq ? 2 : 1);
    assert(*len >= 0);
    memcpy(dat, buf, *len);
    return 0;
}
