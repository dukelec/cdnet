/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

/*
 * Neighbor Discovery for IP version 6 (IPv6)
 *
 * Refer to:
 *   https://tools.ietf.org/html/rfc4861
 *
 * Only support NS and NA Message, and compressed in 6LoCD format
 *
 * We don't use solicited-node multicast address,
 * it's useless for CDBUS, so we use link-local address instead.
 *
 * The source and dest address for NA message are inverted,
 * because we don't want interrupt all nodes on the same net.
 */

/*
 * NS type:
 *   MAC (IP): 255 (::) -> [XX] (fe80::[XX])
 *   pkt_type = LO_NH_ICMP;
 *   icmp_type = ND_TYPE_NS;
 *   6 bytes identify
 *
 * NA type: (send to self MAC, prevent filtering)
 *   MAC (IP): 255 (::) -> [XX] (fe80::[XX])
 *   pkt_type = LO_NH_ICMP;
 *   icmp_type = ND_TYPE_NA;
 *   6 bytes identify
 *   byte0: RSO = 'bx00
 *
 *
 * P.S.:
 *   [XX] is current MAC or target MAC, and it's not equal to 255
 *
 *   6 bytes identify: (determine whether the packet is sent by us)
 *   only use first 4 bytes currently
 */

/*
 * Avoid send collision by lower layer (always enable if sender id = 255):
 *   wait for bus idle
 *   wait a random time, if bus busy once, go back to: "wait for bus idle"
 *   send packet
 *
 * Address negotiation work flow (periodic execution):
 *
 * BEGIN:
 *   target_addr = self_mac == 255 ? gen_random_addr() : self_mac
 *   identify = random uint32 number
 *   set_mac_filter(target_addr)
 *   next: SEND_NS
 *
 * SEND_NS:
 *   send NS packet
 *   next: WAIT_RX
 *
 * WAIT_RX:
 *   if self_mac == 255:
 *     if receive NS or NA from others:
 *       next: BEGIN
 *     else if self NS timeout: (bus data collision)
 *       next: SEND_NS
 *     else if wait NA timeout:
 *       update self_mac with target_addr
 *       next: IDLE
 *
 *   else self_mac != 255:
 *     while receive NS from others:
 *       send NA back
 *     if receive NA from others:
 *       set self_mac = 255
 *       next: BEGIN
 *     else if self NS timeout:
 *       next: SEND_NS
 *     else if wait NA from others timeout:
 *       next: IDLE (keep current address)
 *
 * IDLE:
 *   while receive NS from other node:
 *     send NA back
 *   // avoid received NA
 */

#include "common.h"
#include "6lo_nd.h"


void lo_nd_init(lo_nd_t *intf, lo_intf_t *lo_intf, lo_dispr_t *dispr)
{
#ifdef USE_DYNAMIC_INIT
    intf->state = ND_BEGIN;
#endif

#if (ND_TIMES == 0)
    intf->state = ND_IDLE;
#endif

    intf->identify = rand();
    intf->times = ND_TIMES;

    intf->lo_intf = lo_intf;

    intf->ns_ser.type = ND_TYPE_NS;
    intf->na_ser.type = ND_TYPE_NA;
#ifdef USE_DYNAMIC_INIT
    list_head_init(&intf->ns_ser.A_head);
    list_head_init(&intf->na_ser.A_head);
#endif

#if (ND_TIMES != 0)
    list_put(&dispr->icmp_ser_head, &intf->na_ser.node);
#endif
    list_put(&dispr->icmp_ser_head, &intf->ns_ser.node);
}


static int send_ns(lo_nd_t *lo_nd)
{
    lo_packet_t *pkt;
    lo_intf_t *lo_intf = lo_nd->lo_intf;
    list_node_t *node = list_get(lo_intf->free_head);
    if (!node)
        return -1;
    pkt = container_of(node, lo_packet_t, node);

    pkt->src_addr_type = LO_ADDR_UNSP;
    pkt->src_mac = 255;
    pkt->dst_addr_type = LO_ADDR_LL0;
    pkt->dst_mac = lo_nd->target_addr;

    pkt->pkt_type = LO_NH_ICMP;
    pkt->icmp_type = ND_TYPE_NS;

    pkt->dat_len = 6;
    *(uint32_t *) &pkt->dat[0] = lo_nd->identify;

    list_put(&lo_intf->tx_head, node);
    return 0;
}

#if (ND_TIMES != 0)
static void ns_ser_task(lo_nd_t *lo_nd)
#else
void ns_ser_task(lo_nd_t *lo_nd)
#endif
{
    lo_intf_t *lo_intf = lo_nd->lo_intf;

    while (true) {
        list_node_t *node = list_get(&lo_nd->ns_ser.A_head);
        if (!node)
            break;
        lo_packet_t *pkt = container_of(node, lo_packet_t, node);

        if (pkt->src_mac != 255 || pkt->dat_len != 6)
            goto free_node;

        if (*(uint32_t *) &pkt->dat[0] == lo_nd->identify) {
            lo_nd->if_get_self = true;
            goto free_node;
        }

        if (lo_intf->mac != 255) {
            /*
            pkt->src_addr_type = LO_ADDR_UNSP;
            pkt->src_mac = 255;
            pkt->dst_addr_type = LO_ADDR_LL0;
            pkt->dst_mac = lo_intf->mac;

            pkt->pkt_type = LO_NH_ICMP;
            */
            pkt->icmp_type = ND_TYPE_NA;

            pkt->dat_len = 7;
            *(uint32_t *) &pkt->dat[0] = lo_nd->identify;
            pkt->dat[6] = 0; // b7: R, b6: S, b5: O

            list_put(&lo_intf->tx_head, node);
            continue;
        } else {
            lo_nd->if_get_others = true;
        }

        free_node:
            list_put(lo_intf->free_head, node);
    }
}

static void na_ser_task(lo_nd_t *lo_nd)
{
    lo_intf_t *lo_intf = lo_nd->lo_intf;

    while (true) {
        list_node_t *node = list_get(&lo_nd->na_ser.A_head);
        if (!node)
            break;
        lo_packet_t *pkt = container_of(node, lo_packet_t, node);

        if (pkt->src_mac != 255 || pkt->dat_len != 7)
            goto free_node;

        if (*(uint32_t *) &pkt->dat[0] != lo_nd->identify)
            lo_nd->if_get_others = true;

        free_node:
            list_put(lo_intf->free_head, node);
    }
}


void nd_task(lo_nd_t *lo_nd)
{
    lo_intf_t *lo_intf = lo_nd->lo_intf;
    cd_intf_t *cd_intf = lo_intf->cd_intf;

    if (lo_nd->state == ND_BEGIN) {
        if (lo_intf->mac != 255) {
            lo_nd->target_addr = lo_intf->mac;
        } else {
            lo_nd->target_addr = rand() / (RAND_MAX / 255);
            if (lo_nd->target_addr == 255)
                lo_nd->target_addr = 254;
        }
        lo_nd->identify = rand();

        cd_intf->set_mac_filter(cd_intf, lo_nd->target_addr);
        lo_nd->state = ND_SEND_NS;
    }

    if (lo_nd->state == ND_SEND_NS) {
        send_ns(lo_nd);
        lo_nd->if_get_self = false;
        lo_nd->if_get_others = false;
        lo_nd->t_last = get_systick();
        lo_nd->state = ND_WAIT_RX;
    }

    if (lo_nd->state == ND_WAIT_RX) {
        uint32_t t_now = get_systick();

        if (lo_nd->if_get_others) {
            // receive others NS, NA if mac == 255;
            // or receive others NA if mac != 255:
            if (lo_intf->mac != 255)
                d_info("nd: mac: %02x -> ff\n", lo_intf->mac);
            lo_intf->mac = 255;
            lo_nd->state = ND_BEGIN;
        } else if (!lo_nd->if_get_self &&
                t_now - lo_nd->t_last > ND_NS_TIMEOUT) {
            lo_nd->state = ND_SEND_NS;
        } else if (t_now - lo_nd->t_last > ND_NA_TIMEOUT) {
            if (lo_intf->mac == 255)
                d_info("nd: mac: ff -> %02x\n", lo_nd->target_addr);
            lo_intf->mac = lo_nd->target_addr;
            lo_nd->state = ND_IDLE;
            lo_nd->t_last = get_systick();
            d_debug("nd: next IDLE, times: %d\n", lo_nd->times);
        }
    }

    if (lo_nd->state == ND_IDLE) {
        if (lo_nd->times != 0) {
            if (get_systick() - lo_nd->t_last > ND_INTERVAL) {
                if (lo_nd->times > 0)
                    lo_nd->times--;
                lo_nd->state = ND_BEGIN;
            }
        }
    }

    ns_ser_task(lo_nd);
    na_ser_task(lo_nd);
}

