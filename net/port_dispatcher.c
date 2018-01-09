/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "common.h"
#include "cdnet.h"
#include "port_dispatcher.h"


void port_dispatcher_task(port_dispr_t *dispr)
{
    cdnet_intf_t *intf = dispr->net_intf;

    // rx:

    cdnet_rx(intf);

    while (true) {
        list_node_t *rx_node = list_get(&intf->rx_head);
        if (!rx_node)
            break;
        cdnet_packet_t *pkt = container_of(rx_node, cdnet_packet_t, node);

        if (intf->mac != 255 && !pkt->is_level2) {

            if (pkt->src_port < CDNET_DEF_PORT &&
                    pkt->dst_port >= CDNET_DEF_PORT) {
                list_node_t *item = dispr->udp_req_head.first;
                while (item) {
                    udp_req_t *udp_req = container_of(item, udp_req_t, node);
                    if (pkt->dst_port >= udp_req->begin &&
                            pkt->dst_port < udp_req->end) {
                        if (udp_req->A_en) {
                            list_put(&udp_req->A_head, rx_node);
                            rx_node = NULL;
                        }
                        break;
                    }
                    item = item->next;
                }
            } else if (pkt->src_port >= CDNET_DEF_PORT &&
                    pkt->dst_port < CDNET_DEF_PORT) {
                list_node_t *item = dispr->udp_ser_head.first;
                while (item) {
                    udp_ser_t *udp_ser = container_of(item, udp_ser_t, node);
                    if (pkt->dst_port == udp_ser->port) {
                        list_put(&udp_ser->A_head, rx_node);
                        rx_node = NULL;
                        break;
                    }
                    item = item->next;
                }
            }
        }

        if (rx_node) {
            list_put(intf->free_head, rx_node);
            d_debug("port_dispr: drop rx...\n");
        }
    }

    // tx:

    {
        list_node_t *item = dispr->udp_req_head.first;
        while (item) {
            udp_req_t *udp_req = container_of(item, udp_req_t, node);
            while (true) {
                list_node_t *node = list_get(&udp_req->V_head);
                if (!node)
                    break;
                cdnet_packet_t *pkt = container_of(node, cdnet_packet_t, node);
                pkt->is_level2 = false;
                cdnet_fill_src_addr(intf, pkt);
                pkt->dst_port = udp_req->cur++;
                if (udp_req->cur >= udp_req->end)
                    udp_req->cur = udp_req->begin;
                list_put(&intf->tx_head, node);
            }
            item = item->next;
        }
    }

    while (true) {
        list_node_t *node = list_get(&dispr->V_ser_head);
        if (!node)
            break;
        cdnet_packet_t *pkt = container_of(node, cdnet_packet_t, node);
        cdnet_exchange_src_dst(intf, pkt);
        list_put(&intf->tx_head, node);
    }

    cdnet_tx(intf);
}

