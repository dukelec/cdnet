/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

/*
 * 6LoCD: Compression Format for IPv6 Datagrams over CDBUS
 *
 * This format is almost the same as 6LoWPAN, except:
 *
 *   SAC=1 SAM=10:
 *     6LoWPAN:
 *       IID mapping given by: 0000:00ff:fe00:XXXX
 *     6LoCD:
 *       Site and IID mapping given by: XX:0000:00ff:fe00:00XX
 *
 *   Not use the 4 bits compression for UDP Ports
 *
 * Some fields in the IPHC header are not support and fixed by now,
 * and only support address compression format we need.
 *
 * NH_ICMP is defined by us, refer to this expired draft:
 *   https://tools.ietf.org/html/draft-oflynn-6lowpan-icmphc-00
 *
 * 6LoWPAN reference:
 *   https://tools.ietf.org/html/rfc6282
 */

#include "6lo.h"

#define assert(expr) { if (!(expr)) return -1; }


void lo_intf_init(lo_intf_t *intf, list_head_t *free_head,
    cd_intf_t *cd_intf, uint8_t mac)
{
    intf->mac = mac;    // set to 255 if auto alloc IP addr
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->site_id = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
#endif
}

static int lo_to_frame(const lo_packet_t *pkt, uint8_t *buf)
{
    int i;
    uint8_t *buf_s = buf;

    // CDBUS frame header: [from, to, length]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // skip length

    // IPHC: TF = 'b11, NH = 'b1, HLIM = 'b11, CID = 'b0
    *buf++ = 0x7f;
    *buf++ = pkt->src_addr_type << 4 | pkt->dst_addr_type;

    switch (pkt->src_addr_type) {
    case LO_ADDR_UNSP:
        assert(pkt->src_mac == 0xff);
        break;
    case LO_ADDR_LL0:
        assert(pkt->src_mac != 0xff);
        break;
    case LO_ADDR_UGC16:
        *buf++ = pkt->src_addr[7];
        *buf++ = pkt->src_addr[15];
        assert(pkt->src_mac != 0xff);
        break;
    case LO_ADDR_UG128:
        for (i = 0; i < 16; i++)
            *buf++ = pkt->src_addr[i];
        assert(pkt->src_mac != 0xff);
        break;
    default:
        return -1;
    }

    switch (pkt->dst_addr_type) {
    case LO_ADDR_LL0:
        assert(pkt->dst_mac != 0xff);
        break;
    case LO_ADDR_UGC16:
        *buf++ = pkt->dst_addr[7];
        *buf++ = pkt->dst_addr[15];
        assert(pkt->dst_mac != 0xff);
        break;
    case LO_ADDR_UG128:
    case LO_ADDR_M128:
        for (i = 0; i < 16; i++)
            *buf++ = pkt->dst_addr[i];
        break;
    case LO_ADDR_M8:
        *buf++ = pkt->dst_addr[15];
        break;
    case LO_ADDR_M32:
        *buf++ = pkt->dst_addr[1];
        *buf++ = pkt->dst_addr[13];
        *buf++ = pkt->dst_addr[14];
        *buf++ = pkt->dst_addr[15];
        break;
    default:
        return -1;
    }

    switch (pkt->pkt_type) {
    case LO_NH_UDP:
        {
            uint8_t *udp_type = buf++;
            *udp_type = LO_NH_UDP;
            if (pkt->src_udp_port >> 8 == 0xf0) {
                *udp_type |= 0x2;
                *buf++ = pkt->src_udp_port & 0xff;
            } else {
                *buf++ = pkt->src_udp_port >> 8;
                *buf++ = pkt->src_udp_port & 0xff;
            }
            if (pkt->dst_udp_port >> 8 == 0xf0) {
                *udp_type |= 0x1;
                *buf++ = pkt->dst_udp_port & 0xff;
            } else {
                *buf++ = pkt->dst_udp_port >> 8;
                *buf++ = pkt->dst_udp_port & 0xff;
            }
        }
        break;
    case LO_NH_ICMP:
        *buf++ = LO_NH_ICMP;
        *buf++ = pkt->icmp_type;
        break;
    default:
        return -1;
    }

    if (buf - buf_s + pkt->dat_len > 256)
        return -1;

    for (i = 0; i < pkt->dat_len; i++)
        *buf++ = pkt->dat[i];
    *(buf_s + 2) = buf - buf_s - 3;

    return 0;
}

static int lo_from_frame(const uint8_t *buf, lo_packet_t *pkt)
{
    int i;
    const uint8_t *buf_s = buf;
    uint8_t pkt_type;
    uint8_t payload_len;

    // CDBUS frame header: [from, to, length]
    pkt->src_mac = *buf++;
    pkt->dst_mac = *buf++;
    payload_len = *buf++;

    // IPHC: TF = 'b11, NH = 'b1, HLIM = 'b11, CID = 'b0
    if (*buf++ != 0x7f)
        return -1;
    pkt->src_addr_type = *buf >> 4;
    pkt->dst_addr_type = *buf++ & 0xf;

    switch (pkt->src_addr_type) {
    case LO_ADDR_UNSP:
        assert(pkt->src_mac == 0xff);
        break;
    case LO_ADDR_LL0:
        assert(pkt->src_mac != 0xff);
        break;
    case LO_ADDR_UGC16:
        pkt->src_addr[7] = *buf++;
        pkt->src_addr[15] = *buf++;
        assert(pkt->src_mac != 0xff);
        break;
    case LO_ADDR_UG128:
        for (i = 0; i < 16; i++)
            pkt->src_addr[i] = *buf++;
        break;
    default:
        return -1;
    }

    switch (pkt->dst_addr_type) {
    case LO_ADDR_LL0:
        assert(pkt->dst_mac != 0xff);
        break;
    case LO_ADDR_UGC16:
        pkt->dst_addr[7] = *buf++;
        pkt->dst_addr[15] = *buf++;
        assert(pkt->dst_mac != 0xff);
        break;
    case LO_ADDR_UG128:
        // TODO: same assert as UGC16
    case LO_ADDR_M128:
        for (i = 0; i < 16; i++)
            pkt->dst_addr[i] = *buf++;
        break;
    case LO_ADDR_M8:
        pkt->dst_addr[15] = *buf++;
        break;
    case LO_ADDR_M32:
        pkt->dst_addr[1] = *buf++;
        pkt->dst_addr[13] = *buf++;
        pkt->dst_addr[14] = *buf++;
        pkt->dst_addr[15] = *buf++;
        break;
    default:
        return -1;
    }

    pkt_type = *buf++;
    if ((pkt_type & 0xfc) == LO_NH_UDP) {
        pkt->pkt_type = LO_NH_UDP;
        if (pkt_type & 0x2) {
            pkt->src_udp_port = 0xf000 | *buf++;
        } else {
            pkt->src_udp_port = *buf++ << 8;
            pkt->src_udp_port |= *buf++;
        }
        if (pkt_type & 0x1) {
            pkt->dst_udp_port = 0xf000 | *buf++;
        } else {
            pkt->dst_udp_port = *buf++ << 8;
            pkt->dst_udp_port |= *buf++;
        }
    } else if (pkt_type == LO_NH_ICMP) {
        pkt->pkt_type = LO_NH_ICMP;
        pkt->icmp_type = *buf++;
    } else
        return -1;

    pkt->dat_len = payload_len - (buf - buf_s - 3);
    for (i = 0; i < pkt->dat_len; i++)
        pkt->dat[i] = *buf++;

    return 0;
}


// helper

void lo_exchange_src_dst(lo_intf_t *intf, lo_packet_t *pkt)
{
    uint8_t tmp_addr_type;
    uint8_t tmp_addr[16];
    uint8_t tmp_mac;
    uint16_t tmp_port;

    tmp_port = pkt->src_udp_port;
    pkt->src_udp_port = pkt->dst_udp_port;
    pkt->dst_udp_port = tmp_port;

    tmp_addr_type = pkt->src_addr_type;
    memcpy(tmp_addr, pkt->src_addr, 16);
    tmp_mac = pkt->src_mac;

    pkt->src_addr_type = pkt->dst_addr_type;
    memcpy(pkt->src_addr, pkt->dst_addr, 16);
    pkt->src_mac = pkt->dst_mac;

    pkt->dst_addr_type = tmp_addr_type;
    memcpy(pkt->dst_addr, tmp_addr, 16);
    pkt->dst_mac = tmp_mac;

    // if src is unspecified
    if (pkt->dst_addr_type == LO_ADDR_UNSP) {
        pkt->dst_addr_type = LO_ADDR_M8;
        pkt->dst_addr[15] = 0x01;
        pkt->dst_mac = 0xff;
    }

    // if dst is multicast
    if (pkt->src_addr_type & 0x8) {
        switch (pkt->src_addr_type) {
        case LO_ADDR_M8:
            pkt->src_addr_type = LO_ADDR_LL0;
            break;
        case LO_ADDR_M32:
        case LO_ADDR_M128:
            pkt->src_addr_type = LO_ADDR_UGC16;
            pkt->src_addr[7] = intf->site_id;
            pkt->src_addr[15] = intf->mac;
            break;
        }
        pkt->src_mac = intf->mac;
    }
}

void lo_fill_src_addr(lo_intf_t *intf, lo_packet_t *pkt)
{
    if (pkt->dst_addr_type == LO_ADDR_LL0 ||
            pkt->dst_addr_type == LO_ADDR_M8) {
        pkt->src_addr_type = LO_ADDR_LL0;
        pkt->src_mac = intf->mac;
    } else {
        pkt->src_addr_type = LO_ADDR_UGC16;
        pkt->src_addr[7] = intf->site_id;
        pkt->src_addr[15] = intf->mac;
        pkt->src_mac = intf->mac;
    }
}

//

void lo_rx(lo_intf_t *intf)
{
    list_node_t *cd_node, *lo_node;
    cd_frame_t *cd_frame;
    lo_packet_t *lo_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;

    // TODO: add address filter
    while (true) {
        if (!intf->free_head->first)
            break;

        cd_node = cd_intf->get_rx_node(cd_intf);
        if (!cd_node)
            break;

        lo_node = list_get(intf->free_head);
        lo_pkt = container_of(lo_node, lo_packet_t, node);
        cd_frame = container_of(cd_node, cd_frame_t, node);
        if (lo_from_frame(cd_frame->dat, lo_pkt) == 0) {
            list_put(&intf->rx_head, lo_node);
        } else {
            list_put(intf->free_head, lo_node);
            d_error("lo %p: lo_from_frame failed\n", intf);
        }
        cd_intf->put_free_node(cd_intf, cd_node);
    }
}

void lo_tx(lo_intf_t *intf)
{
    list_node_t *cd_node, *lo_node;
    cd_frame_t *cd_frame;
    lo_packet_t *lo_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;

    while (true) {
        lo_node = list_get(&intf->rx_head);
        if (!lo_node)
            break;
        list_put(intf->free_head, lo_node);
    }
    while (true) {
        if (!intf->tx_head.first)
            break;

        cd_node = cd_intf->get_free_node(cd_intf);
        if (!cd_node)
            break;

        cd_frame = container_of(cd_node, cd_frame_t, node);
        lo_node = list_get(&intf->tx_head);
        lo_pkt = container_of(lo_node, lo_packet_t, node);

        if (lo_to_frame(lo_pkt, cd_frame->dat) == 0) {
            cd_intf->put_tx_node(cd_intf, cd_node);
        } else {
            cd_intf->put_free_node(cd_intf, cd_node);
            d_error("lo %p: lo_to_frame failed\n", intf);
        }
        list_put(intf->free_head, lo_node);
    }
}

