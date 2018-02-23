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
        cd_intf_t *cd_intf, cdnet_addr_t *addr)
{
    if (!intf->name)
        intf->name = "cdnet";
    intf->addr.net = addr->net;
    intf->addr.mac = addr->mac; // 255: unspecified
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->l0_last_port = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
#endif

    cdnet_seq_init(intf);
}


// helper

void cdnet_exchg_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    cdnet_addr_t tmp_addr;
    uint8_t tmp_mac;
    uint16_t tmp_port;

    tmp_mac = pkt->src_mac;
    pkt->src_mac = pkt->dst_mac;
    pkt->dst_mac = tmp_mac;

    if (pkt->src_mac == 255)
        pkt->src_mac = intf->addr.mac;

    if (pkt->level == CDNET_L1 && pkt->multi) {
        tmp_addr = pkt->src_addr;
        pkt->src_addr = pkt->dst_addr;
        pkt->dst_addr = tmp_addr;
        if (pkt->multi & CDNET_MULTI_CAST) {
            pkt->multi &= (~CDNET_MULTI_CAST) & 3;
            pkt->src_addr = intf->addr;
        }
    }

    if (pkt->level != CDNET_L2) {
        tmp_port = pkt->src_port;
        pkt->src_port = pkt->dst_port;
        pkt->dst_port = tmp_port;
    }
}

void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    pkt->src_mac = intf->addr.mac;
    if (pkt->level == CDNET_L1 && pkt->multi >= CDNET_MULTI_NET)
        pkt->src_addr = intf->addr;
}

//

void cdnet_rx(cdnet_intf_t *intf)
{
    cd_frame_t *frame;
    cdnet_packet_t *pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    while (true) {
        if (!intf->free_head->first) {
            dd_warn(intf->name, "rx: no free pkt\n");
            return;
        }

        frame = cd_intf->get_rx_frame(cd_intf);
        if (!frame)
            return;
        pkt = cdnet_packet_get(intf->free_head);

        if ((frame->dat[3] & 0xc0) == 0xc0)
            ret_val = cdnet_l2_from_frame(intf, frame->dat, pkt);
        else if (frame->dat[3] & 0x80)
            ret_val = cdnet_l1_from_frame(intf, frame->dat, pkt);
        else
            ret_val = cdnet_l0_from_frame(intf, frame->dat, pkt);

        cd_intf->put_free_frame(cd_intf, frame);

        if (ret_val != 0) {
            dd_error(intf->name, "rx: from_frame err\n");
            cdnet_list_put(intf->free_head, &pkt->node);
            return;
        }
        if (pkt->multi & CDNET_MULTI_CAST) {
            dd_error(intf->name, "rx: not support multicast yet\n");
            cdnet_list_put(intf->free_head, &pkt->node);
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
        if (pkt->level != CDNET_L0 && pkt->seq) {
            cdnet_seq_rx_handle(intf, pkt);
            return;
        }

        // send left pkt to upper layer directly
        cdnet_list_put(&intf->rx_head, &pkt->node);
    }
}

void cdnet_tx(cdnet_intf_t *intf)
{
    cdnet_seq_tx_task(intf);
}
