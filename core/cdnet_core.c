/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet_core.h"
#include "cd_debug.h"


// simplified for basic use, override for full processing
__weak cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    cdn_intf_t *intf = &ns->intfs[0];
    pkt->src.addr[0] = pkt->dst.addr[0] != 0xf0 ? pkt->dst.addr[0] : 0x80; // or 0xa0
    pkt->src.addr[1] = intf->net;
    pkt->src.addr[2] = intf->mac;
    pkt->_s_mac = intf->mac;
    pkt->_d_mac = pkt->dst.addr[2]; // or default route for unique local
    return intf;
}

static cdn_sock_t *cdn_sock_search(cdn_ns_t *ns, uint16_t port)
{
    list_node_t *pos;
    list_for_each_ro(&ns->socks, pos) {
        cdn_sock_t *sock = list_entry(pos, cdn_sock_t);
        if (sock->port == port) {
            return sock;
        }
    }
    return NULL;
}

static int cdn_sock_insert(cdn_sock_t *sock)
{
    if (cdn_sock_search(sock->ns, sock->port))
        return -1;
    list_put(&sock->ns->socks, &sock->node);
    return 0;
}


void cdn_routine(cdn_ns_t *ns)
{
    // rx
    for (int i = 0; i < CDN_INTF_MAX; i++) {
        cdn_intf_t *intf = &ns->intfs[i];
        cd_frame_t *frame;
        cdn_pkt_t *pkt;
        cd_dev_t *dev = intf->dev;
        int ret;

        while (true) {
            if (!ns->free_pkts->len) {
                d_warn("rx: no free pkt\n");
                break;
            }

            frame = dev->get_rx_frame(dev);
            if (!frame)
                break;
            pkt = cdn_pkt_get(ns->free_pkts);
            cdn_init_pkt(pkt);
            pkt->_l_net = intf->net;

            if (!(frame->dat[3] & 0x80)) { // rx l0
#ifdef CDN_L0_C
                if (frame->dat[3] & 0x40) // reply
                    pkt->_l0_lp = intf->_l0_lp;
#endif
                // addition in: _l_net, _l0_lp (central only)
                ret = cdn0_from_frame(frame->dat, pkt);

                if (!ret) {
                    cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                    if (!ret && sock && !sock->tx_only) {
                        cdn_list_put(&sock->rx_head, &pkt->node);
                    } else {
                        d_verbose("rx: l0 no sock\n");
                        cdn_list_put(ns->free_pkts, &pkt->node);
                    }
                } else {
                    d_verbose("rx: l0 frame err: %d\n", ret);
                    cdn_list_put(ns->free_pkts, &pkt->node);
                }

            } else { // rx l1
                // addition in: _l_net
                ret = cdn1_from_frame(frame->dat, pkt);

                if (!ret) {
                    // TODO: check dst net match or not
                    cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                    if (sock && !sock->tx_only) {
                        cdn_list_put(&sock->rx_head, &pkt->node);
                    } else {
                        d_verbose("rx: l1 no sock\n");
                        cdn_list_put(ns->free_pkts, &pkt->node);
                    }
                } else {
                    d_verbose("rx: l1 frame err: %d\n", ret);
                    cdn_list_put(ns->free_pkts, &pkt->node);
                }
            }

            dev->put_free_frame(dev, frame);
        }
    }
}


int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    int ret;
    cdn_intf_t *intf = cdn_route(ns, pkt);
    if (!intf) {
        d_verbose("tx: no intf found\n");
        return CDN_RET_ROUTE_ERR;
    }

    cd_frame_t *frame = intf->dev->get_free_frame(intf->dev);
    if (!frame)
        return CDN_RET_NO_FREE;

    if (pkt->src.addr[0] == 0) {
#ifdef CDN_L0_C
        if (pkt->src.port == CDN_DEF_PORT)
            intf->_l0_lp = pkt->dst.port;
#endif
        ret = cdn0_to_frame(pkt, frame->dat);
    } else {
        ret = cdn1_to_frame(pkt, frame->dat);
    }

    if (ret) {
        intf->dev->put_free_frame(intf->dev, frame);
        return CDN_RET_FMT_ERR;
    } else {
        intf->dev->put_tx_frame(intf->dev, frame);
    }
    return 0;
}


int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    if (pkt->dst.addr[0] == 0x10) { // localhost
        cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
        if (sock && !sock->tx_only) {
            memcpy(pkt->src.addr, pkt->dst.addr, 3);
            cdn_list_put(&sock->rx_head, &pkt->node);
        } else {
            d_verbose("tx: localhost no sock\n");
            cdn_list_put(ns->free_pkts, &pkt->node);
        }
        return 0;
    }

    int ret = cdn_send_frame(ns, pkt);
    pkt->ret = 0x80 | ret;
    if (!(pkt->conf & CDN_CONF_NOT_FREE))
        cdn_list_put(ns->free_pkts, &pkt->node);
    return ret;
}


int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt)
{
    pkt->src.port = sock->port;
    return cdn_send_pkt(sock->ns, pkt);
}

cdn_pkt_t *cdn_sock_recvfrom(cdn_sock_t *sock)
{
    if (!sock->rx_head.len)
        return NULL;
    return cdn_pkt_get(&sock->rx_head);
}

int cdn_sock_bind(cdn_sock_t *sock)
{
    return cdn_sock_insert(sock);
}

int cdn_add_intf(cdn_ns_t *ns, cd_dev_t *dev, uint8_t net, uint8_t mac)
{
    for (int i = 0; i < CDN_INTF_MAX; i++) {
        if (!ns->intfs[i].dev) {
            ns->intfs[i].dev = dev;
            ns->intfs[i].net = net;
            ns->intfs[i].mac = mac;
            return 0;
        }
    }
    return -1;
}

void cdn_init_ns(cdn_ns_t *ns, list_head_t *free_head)
{
    memset(ns, 0, sizeof(cdn_ns_t));
    ns->free_pkts = free_head;
}
