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


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
        cd_intf_t *cd_intf, uint8_t mac)
{
    intf->mac = mac;    // set to 255 if auto alloc IP addr
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->net_id = 0;
    intf->last_basic_port = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    list_head_init(&intf->rx_frag_head);
    list_head_init(&intf->tx_frag_head);
#endif
}

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


static int cdnet_l0_l1_to_frame(cdnet_intf_t *intf,
        cdnet_packet_t *pkt, uint8_t *buf)
{
    int i;
    int ret;
    uint8_t src_port_size;
    uint8_t dst_port_size;

    uint8_t *buf_s = buf;
    uint8_t *hdr = buf + 3;

    assert(!pkt->is_level2);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end

    // level0
    if (!pkt->is_multi_net && !pkt->is_multicast &&
            ((pkt->src_port == CDNET_DEF_PORT && pkt->dst_port <= 63) ||
                    pkt->dst_port == CDNET_DEF_PORT)) {

        if (pkt->src_port == CDNET_DEF_PORT) { // out request
            intf->last_level0_port = pkt->dst_port;
            *buf++ = pkt->dst_port; // hdr
        } else { // out reply
            if (pkt->dat_len >= 1 && pkt->dat[0] <= 31) {
                // share first byte
                pkt->dat[0] |= HDR_L0_REPLY | HDR_L0_SHARE;
            } else {
                *buf++ = HDR_L0_REPLY; // hdr
            }
        }

    // level1
    } else {
        *buf++ = HDR_L1_L2; // hdr
        if (pkt->is_multi_net) {
            *hdr |= HDR_L1_MULTI_NET;
            *buf++ = pkt->src_addr[0];
            *buf++ = pkt->src_addr[1];
            if (!pkt->is_multicast) {
                *buf++ = pkt->dst_addr[0];
                *buf++ = pkt->dst_addr[1];
            }
        }
        if (pkt->is_multicast) {
            *hdr |= HDR_L1_MULTICAST;
            *buf++ = pkt->multicast_id & 0xff;
            *buf++ = pkt->multicast_id >> 8;
        }

        ret = cal_port_val(pkt->src_port, pkt->dst_port,
                &src_port_size, &dst_port_size);
        if (ret < 0)
            return -1;

        *hdr |= ret;
        if (src_port_size >= 1)
            *buf++ = pkt->src_port & 0xff;
        if (src_port_size == 2)
            *buf++ = pkt->src_port >> 8;
        if (dst_port_size >= 1)
            *buf++ = pkt->dst_port & 0xff;
        if (dst_port_size == 2)
            *buf++ = pkt->dst_port >> 8;
    }

    assert(buf - buf_s + pkt->dat_len <= 256);
    for (i = 0; i < pkt->dat_len; i++)
        *buf++ = pkt->dat[i];
    *(buf_s + 2) = buf - buf_s - 3;
    return 0;
}

static int cdnet_l0_l1_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    int i;
    uint8_t src_port_size;
    uint8_t dst_port_size;

    const uint8_t *buf_s = buf;
    const uint8_t *hdr = buf + 3;
    uint8_t *cpy_to = pkt->dat;
    uint8_t cpy_len;
    uint8_t tmp_len;

    assert((*hdr & 0xc0) != 0xc0);
    pkt->is_level2 = false;

    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    tmp_len = *buf++;
    assert(tmp_len >= 1);
    pkt->dat_len = 0;
    buf++; // skip hdr

    pkt->is_multi_net = false;
    pkt->is_multicast = false;

    if (!(*hdr & HDR_L1_L2)) { // level0 format
        pkt->dat_len = tmp_len - 1;
        cpy_len = pkt->dat_len;

        if (*hdr & HDR_L0_REPLY) { // in reply
            pkt->src_port = intf->last_level0_port;
            pkt->dst_port = CDNET_DEF_PORT;
            if (*hdr & HDR_L0_SHARE) {
                pkt->dat_len = tmp_len;
                cpy_len = pkt->dat_len - 1;
                *cpy_to++ = *hdr & 0x1f;
            }
        } else { // in request
            pkt->src_port = CDNET_DEF_PORT;
            pkt->dst_port = *hdr;
        }
    } else { // level1 format
        if (*hdr & HDR_L1_MULTI_NET) {
            pkt->is_multi_net = true;
            pkt->src_addr[0] = *buf++;
            pkt->src_addr[1] = *buf++;
            if (!(*hdr & HDR_L1_MULTICAST)) {
                pkt->dst_addr[0] = *buf++;
                pkt->dst_addr[1] = *buf++;
            }
        }
        if (*hdr & HDR_L1_MULTICAST) {
            pkt->is_multicast = true;
            pkt->multicast_id = *buf++;
            pkt->multicast_id |= *buf++ << 8;
        }

        get_port_size(*hdr & 0x07, &src_port_size, &dst_port_size);

        if (src_port_size >= 1)
            pkt->src_port = *buf++;
        if (src_port_size == 2)
            pkt->src_port |= *buf++ << 8;
        if (dst_port_size >= 1)
            pkt->dst_port = *buf++;
        if (dst_port_size == 2)
            pkt->dst_port |= *buf++ << 8;

        pkt->dat_len = tmp_len - (buf - buf_s - 3);
        cpy_len = pkt->dat_len;
    }

    assert(pkt->dat_len >= 0);
    for (i = 0; i < cpy_len; i++)
        *cpy_to++ = *buf++;
    return 0;
}


static int cdnet_l2_to_frame(cdnet_intf_t *intf,
        cdnet_packet_t *pkt, uint8_t *buf)
{
    int i;
    int ret = 0;
    uint8_t *buf_s = buf;
    uint8_t *hdr = buf + 3;
    int left_data_len;
    uint8_t cpy_len;

    assert(pkt->is_level2);

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end

    *buf++ = HDR_L1_L2 | HDR_L2; // hdr

    // fragmentation

    if (!pkt->in_fragment)
        left_data_len = pkt->dat_len;
    else
        left_data_len = pkt->dat_len - (pkt->frag_at - pkt->dat);

    if (!pkt->in_fragment) {
        pkt->frag_at = pkt->dat;

        if (left_data_len + 1 > 253) { // 1: 1 byte header
            ret = RET_NOT_FINISH;
            cpy_len = 253 - 2; // 2: 2 bytes header include fragment-id
            pkt->in_fragment = true;
            *hdr |= HDR_L2_FRAGMENT;
            *buf++ = pkt->frag_cnt = 0;
        } else {
            cpy_len = left_data_len;
        }
    } else {
        if (left_data_len + 2 > 253) {
            ret = RET_NOT_FINISH;
            cpy_len = 253 - 2;
            *hdr |= HDR_L2_FRAGMENT;
            *buf++ = ++pkt->frag_cnt;
            assert(pkt->frag_cnt != 0);
        } else {
            cpy_len = left_data_len;
            pkt->in_fragment = false;
            *hdr |= HDR_L2_FRAGMENT | HDR_L2_FRAGMENT_END;
            *buf++ = ++pkt->frag_cnt;
            assert(pkt->frag_cnt != 0);
        }
    }

    assert(buf - buf_s + cpy_len <= 256);

    for (i = 0; i < cpy_len; i++)
        *buf++ = *pkt->frag_at++;
    *(buf_s + 2) = buf - buf_s - 3;

    return ret;
}

static int cdnet_l2_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    int i;
    int ret = 0;
    const uint8_t *hdr = buf + 3;
    uint8_t cpy_len;
    uint8_t tmp_len;

    assert((*hdr & 0xc0) == 0xc0);
    pkt->is_level2 = true;

    if (!pkt->in_fragment) {
        pkt->src_mac = *buf++;
        pkt->dst_mac = *buf++;
        tmp_len = *buf++;
        pkt->frag_at = pkt->dat;
        pkt->dat_len = 0;
        pkt->frag_cnt = 0;
        buf++; // skip hdr

        if (*hdr & HDR_L2_FRAGMENT) {
            if (*buf++ != 0) // fragment-id
                return ERR_PKT_ORDER;
            ret = RET_NOT_FINISH;
            pkt->in_fragment = true;
            cpy_len = tmp_len - 2;
        } else {
            cpy_len = tmp_len - 1;
        }

    } else { // segment
        if (pkt->src_mac != *buf++)
            return RET_PKT_NOT_MATCH;
        if (pkt->dst_mac != *buf++)
            return RET_PKT_NOT_MATCH;
        tmp_len = *buf++;
        cpy_len = tmp_len - 2;
        buf++; // skip hdr

        assert(*hdr & HDR_L2_FRAGMENT);
        if (*hdr & HDR_L2_FRAGMENT_END)
            pkt->in_fragment = false;
        else
            ret = RET_NOT_FINISH;

        if (++pkt->frag_cnt != *buf++)
            return ERR_PKT_ORDER;
    }

    pkt->dat_len += cpy_len;
    for (i = 0; i < cpy_len; i++)
        *pkt->frag_at++ = *buf++;
    return ret;
}


// helper

void cdnet_exchange_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    uint8_t tmp_addr[2];
    uint8_t tmp_mac;
    uint16_t tmp_port;

    tmp_mac = pkt->src_mac;
    pkt->src_mac = pkt->dst_mac;
    pkt->dst_mac = tmp_mac;

    if (pkt->is_multicast)
        pkt->src_mac = intf->mac;

    if (pkt->is_multi_net) {
        memcpy(tmp_addr, pkt->src_addr, 2);
        memcpy(pkt->src_addr, pkt->dst_addr, 2);
        memcpy(pkt->dst_addr, tmp_addr, 2);
        if (pkt->is_multicast) {
            pkt->src_addr[0] = intf->net;
            pkt->src_addr[1] = intf->mac;
        }
    }

    tmp_port = pkt->src_port;
    pkt->src_port = pkt->dst_port;
    pkt->dst_port = tmp_port;
}

void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    pkt->src_mac = intf->mac;

    if (pkt->is_multi_net) {
        pkt->src_addr[0] = intf->net;
        pkt->src_addr[1] = intf->mac;
    }
}

//

void cdnet_rx(cdnet_intf_t *intf)
{
    list_node_t *cd_node, *net_node;
    cd_frame_t *cd_frame;
    cdnet_packet_t *net_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    // TODO: add address filter
    while (true) {
        if (!intf->free_head->first)
            break;

        cd_node = cd_intf->get_rx_node(cd_intf);
        if (!cd_node)
            break;
        cd_frame = container_of(cd_node, cd_frame_t, node);

        while (true) {
            if (intf->rx_frag_head.first) {
                net_node = list_get(&intf->rx_frag_head);
                net_pkt = container_of(net_node, cdnet_packet_t, node);
            } else {
                net_node = list_get(intf->free_head);
                net_pkt = container_of(net_node, cdnet_packet_t, node);
                net_pkt->in_fragment = false;
            }

            if ((cd_frame->dat[3] & 0xc0) == 0xc0)
                ret_val = cdnet_l2_from_frame(intf, cd_frame->dat, net_pkt);
            else
                ret_val = cdnet_l0_l1_from_frame(intf, cd_frame->dat, net_pkt);

            // TODO: add timeout counter for pkt
            if (ret_val == RET_PKT_NOT_MATCH)
                continue;

            cd_intf->put_free_node(cd_intf, cd_node);

            if (ret_val == 0) {
                list_put(&intf->rx_head, net_node);
            } else if (ret_val == RET_NOT_FINISH) {
                list_put(&intf->rx_frag_head, net_node);
            } else {
                list_put(intf->free_head, net_node);
                d_error("cdnet %p: cdnet_from_frame failed\n", intf);
            }
            break;
        }
    }
}

void cdnet_tx(cdnet_intf_t *intf)
{
    list_node_t *cd_node, *net_node;
    cd_frame_t *cd_frame;
    cdnet_packet_t *net_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    while (true) {
        net_node = list_get(&intf->rx_head);
        if (!net_node)
            break;
        list_put(intf->free_head, net_node);
    }

    while (true) {
        if (!intf->tx_head.first && !intf->tx_frag_head.first)
            break;

        cd_node = cd_intf->get_free_node(cd_intf);
        if (!cd_node)
            break;

        cd_frame = container_of(cd_node, cd_frame_t, node);
        if (intf->tx_frag_head.first) {
            net_node = list_get(&intf->tx_frag_head);
            net_pkt = container_of(net_node, cdnet_packet_t, node);
        } else {
            net_node = list_get(&intf->tx_head);
            net_pkt = container_of(net_node, cdnet_packet_t, node);
            net_pkt->in_fragment = false;
        }

        if (net_pkt->is_level2)
            ret_val = cdnet_l2_to_frame(intf, net_pkt, cd_frame->dat);
        else
            ret_val = cdnet_l0_l1_to_frame(intf, net_pkt, cd_frame->dat);

        if (ret_val == 0 || ret_val == RET_NOT_FINISH) {
            cd_intf->put_tx_node(cd_intf, cd_node);
        } else {
            cd_intf->put_free_node(cd_intf, cd_node);
            d_error("cdnet %p: cdnet_to_frame failed\n", intf);
        }

        if (ret_val == RET_NOT_FINISH)
            list_put(&intf->tx_frag_head, net_node);
        else
            list_put(intf->free_head, net_node);
    }
}

