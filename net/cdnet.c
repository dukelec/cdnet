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

int cdnet_l0_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);
int cdnet_l1_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);
int cdnet_l2_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);

void cdnet_seq_init(cdnet_intf_t *intf);
void cdnet_p0_request_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_p0_reply_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_seq_rx_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt);
void cdnet_seq_tx_task(cdnet_intf_t *intf);


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
        cd_intf_t *cd_intf, uint8_t mac)
{
    intf->mac = mac; // 255: unspecified
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->net = 0;
    intf->l0_last_port = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
#endif

    cdnet_seq_init(intf);
}


// helper

void cdnet_exchg_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    uint8_t tmp_addr[2];
    uint8_t tmp_mac;
    uint16_t tmp_port;

    tmp_mac = pkt->src_mac;
    pkt->src_mac = pkt->dst_mac;
    pkt->dst_mac = tmp_mac;

    if (pkt->src_mac == 255)
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
    cd_frame_t *frame;
    cdnet_packet_t *pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    if (!intf->free_head->first) {
        d_warn("cdnet %p: no free node for rx\n", intf);
        return;
    }

    cd_node = cd_intf->get_rx_node(cd_intf);
    if (!cd_node)
        return;
    frame = container_of(cd_node, cd_frame_t, node);

    net_node = cdnet_list_get(intf->free_head);
    pkt = container_of(net_node, cdnet_packet_t, node);

    if ((frame->dat[3] & 0xc0) == 0xc0)
        ret_val = cdnet_l2_from_frame(intf, frame->dat, pkt);
    else if (frame->dat[3] & 0x80)
        ret_val = cdnet_l1_from_frame(intf, frame->dat, pkt);
    else
        ret_val = cdnet_l0_from_frame(intf, frame->dat, pkt);

    cd_intf->put_free_node(cd_intf, cd_node);

    if (ret_val != 0) {
        d_error("cdnet %p: cdnet_from_frame failed\n", intf);
        cdnet_list_put(intf->free_head, net_node);
        return;
    }

    if (pkt->level != CDNET_L2 && pkt->dst_port == 0 &&
            pkt->src_port >= CDNET_DEF_PORT) {
        cdnet_p0_request_handle(intf, pkt);
        return;
    }
    if (pkt->level != CDNET_L2 && pkt->src_port == 0 &&
            pkt->dst_port == CDNET_DEF_PORT) {
        cdnet_p0_reply_handle(intf, pkt);
        return;
    }
    if (pkt->level != CDNET_L0 && pkt->is_seq) {
        cdnet_seq_rx_handle(intf, pkt);
        return;
    }

    // send left pkt to upper layer directly
    cdnet_list_put(&intf->rx_head, net_node);
}

void cdnet_tx(cdnet_intf_t *intf)
{
    cdnet_seq_tx_task(intf);
}
