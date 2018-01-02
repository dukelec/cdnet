/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "common.h"
#include "6lo.h"
#include "6lo_dispatcher.h"


void lo_dispatcher_task(lo_dispr_t *dispr)
{
    lo_intf_t *intf = dispr->lo_intf;

    // rx:

    lo_rx(intf);

    while (true) {
        list_node_t *rx_node = list_get(&intf->rx_head);
        if (!rx_node)
            break;
        lo_packet_t *pkt = container_of(rx_node, lo_packet_t, node);

        if (intf->mac != 255 && pkt->pkt_type == LO_NH_UDP) {

            if (pkt->dst_udp_port >= 0xf000) {
                list_node_t *item = dispr->udp_req_head.first;
                while (item) {
                    udp_req_t *udp_req = container_of(item, udp_req_t, node);
                    if (pkt->dst_udp_port >= udp_req->begin &&
                            pkt->dst_udp_port < udp_req->end) {
                        if (udp_req->is_active) {
                            list_put(&udp_req->A_head, rx_node);
                            rx_node = NULL;
                        }
                        break;
                    }
                    item = item->next;
                }
            } else {
                list_node_t *item = dispr->udp_ser_head.first;
                while (item) {
                    udp_ser_t *udp_ser = container_of(item, udp_ser_t, node);
                    if (pkt->dst_udp_port == udp_ser->port) {
                        list_put(&udp_ser->A_head, rx_node);
                        rx_node = NULL;
                        break;
                    }
                    item = item->next;
                }
            }
        } else if (pkt->pkt_type == LO_NH_ICMP) {
            list_node_t *item = dispr->icmp_ser_head.first;
            while (item) {
                icmp_ser_t *icmp_ser = container_of(item, icmp_ser_t, node);
                if (pkt->icmp_type == icmp_ser->type) {
                    list_put(&icmp_ser->A_head, rx_node);
                    rx_node = NULL;
                    break;
                }
                item = item->next;
            }
        }

        if (rx_node)
            list_put(intf->free_head, rx_node); // TODO: return ICMP error
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
                lo_packet_t *pkt = container_of(node, lo_packet_t, node);
                lo_fill_src_addr(intf, pkt);
                pkt->dst_udp_port = udp_req->cur++;
                if (udp_req->cur == udp_req->end)
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
        lo_packet_t *pkt = container_of(node, lo_packet_t, node);
        lo_exchange_src_dst(intf, pkt);
        list_put(&intf->tx_head, node);
    }

    lo_tx(intf);
}

