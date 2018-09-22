/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdnet.h"


int cdnet_l0_to_frame(const cd_sockaddr_t *src, const cd_sockaddr_t *dst,
        const uint8_t *dat, uint8_t len, bool allow_share, uint8_t *frame)
{
    uint8_t *buf = frame;

    assert(dst->addr.cd_addr_type == 0x00 || dst->addr.cd_addr_type == 0x08);
    assert((src->port == CDNET_DEF_PORT && dst->port <= 63) ||
            dst->port == CDNET_DEF_PORT);

    *buf++ = src->addr.cd_addr_mac;
    *buf++ = dst->addr.cd_addr_mac;
    buf++; // fill at end

    if (src->port == CDNET_DEF_PORT) { // out request
                                    // backup dst port if need at outside
        *buf++ = dst->port; // hdr
    } else { // out reply
        if (len >= 1 && dat[0] <= 31) {
            // share first byte
            *buf++ = HDR_L0_REPLY | HDR_L0_SHARE | dat[0];
            len--;
            dat++;
        } else {
            *buf++ = HDR_L0_REPLY; // hdr
        }
    }

    assert(buf - frame + len <= 256);
    *(frame + 2) = buf - frame + len - 3;
    memcpy(buf, dat, len);
    return 0;
}

int cdnet_l0_from_frame(const uint8_t *frame,
        uint8_t local_net, uint16_t last_port,
        cd_sockaddr_t *src, cd_sockaddr_t *dst, uint8_t *dat, uint8_t *len)
{
    const uint8_t *buf = frame;
    const uint8_t *hdr = frame + 3;

    assert(!(*hdr & 0x80));

    src->addr.cd_addr_type = 0;
    src->addr.cd_addr_net = local_net;
    dst->addr.cd_addr_type = 0;
    dst->addr.cd_addr_net = local_net;

    src->addr.cd_addr_mac = *buf++;
    dst->addr.cd_addr_mac = *buf++;
    *len = (*buf++) - 1;
    assert(*len >= 1);
    buf++; // skip hdr

    if (*hdr & HDR_L0_REPLY) { // in reply
        src->port = last_port;
        dst->port = CDNET_DEF_PORT;
        if (*hdr & HDR_L0_SHARE) {
            (*len)--;
            *dat++ = *hdr & 0x1f;
        }
    } else { // in request
        src->port = CDNET_DEF_PORT;
        dst->port = *hdr;
    }

    assert(*len >= 0);
    memcpy(dat, buf, *len);
    return 0;
}
