/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet_dispatch.h"
#include "cd_debug.h"

#ifdef CDN_SEQ
static void p0_service_routine(cdn_ns_t *ns)
{
    cdn_pkt_t *pkt = cdn_sock_recvfrom(&ns->sock0);
    if (!pkt)
        return;

    pkt->src.addr[0] &= 0xf0; // reply without seq

    if (pkt->len == 1 && pkt->dat[0] == 0) { // check
        pkt->dat[0] = 0x80;
        cdn_tgt_t *tgt = cdn_tgt_search(ns, pkt->src.addr[1], pkt->src.addr[2]);

        if (tgt)
            pkt->dat[1] = tgt->rx_seq;
        else
            pkt->dat[1] = 0xff;

        pkt->len = 2;
        pkt->dst = pkt->src;
        cdn_sock_sendto(&ns->sock0, pkt);
        return;

    } else if (pkt->len == 2 && pkt->dat[0] == 0x20) { // set

        cdn_tgt_t *tgt = cdn_tgt_search(ns, pkt->src.addr[1], pkt->src.addr[2]);
        if (tgt) {
            tgt->rx_seq = pkt->dat[1];
        } else { // release oldest
            tgt = &ns->tgts[CDN_TGT_MAX - 1];
            tgt->net = pkt->src.addr[1];
            tgt->mac = pkt->src.addr[2];
            tgt->rx_seq = pkt->dat[1];
            cdn_tgt_search(ns, pkt->src.addr[1], pkt->src.addr[2]); // move forward
        }

        pkt->dat[0] = 0x80;
        pkt->len = 1;
        pkt->dst = pkt->src;
        cdn_sock_sendto(&ns->sock0, pkt);
        return;
    }

    d_debug("p0 ser: ignore\n");
    list_put(&ns->free_pkts, &pkt->node);
}

static int p0_report(cdn_ns_t *ns, cdn_sockaddr_t dst, uint8_t val)
{
    dst.addr[0] &= 0xf0; // reply without seq
#ifdef CDN_L2
    if (dst.addr[0] == 0xc0) {
        dst.addr[0] = 0x80;
        dst.port = CDN_L2_P0_RPT_PORT;
    }
#endif

    cdn_pkt_t *pkt = cdn_pkt_get(&ns->free_pkts);
    if (!pkt)
        return -1;
    cdn_init_pkt(pkt);
    pkt->dat[0] = 0x40;
    pkt->dat[1] = val;
    pkt->len = 2;
    pkt->dst = dst;
    cdn_sock_sendto(&ns->sock0, pkt);
    return 0;
}
#endif


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
            if (!ns->free_pkts.len) {
                d_warn("rx: no free pkt\n");
                break;
            }

            frame = dev->get_rx_frame(dev);
            if (!frame)
                break;
            pkt = cdn_pkt_get(&ns->free_pkts);
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
                        cdn_list_put(&ns->free_pkts, &pkt->node);
                    }
                } else {
                    d_verbose("rx: l0 frame err: %d\n", ret);
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                }

            } else if (!(frame->dat[3] & 0x40)) { // rx l1
                // addition in: _l_net; out: _seq
                ret = cdn1_from_frame(frame->dat, pkt);

                if (!ret) {
                    // TODO: check dst net match or not
                    if (pkt->src.addr[0] & 0x8) { // seq
#ifdef CDN_SEQ
                        cdn_tgt_t *tgt = cdn_tgt_search(ns, pkt->src.addr[1], pkt->src.addr[2]);
                        if (tgt) {
                            if ((pkt->seq & 0x7f) == tgt->rx_seq) {
                                tgt->rx_seq = (tgt->rx_seq + 1) & 0x7f;
                                cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                                if (sock && !sock->tx_only) {
                                    if (pkt->seq & 0x80)
                                        p0_report(ns, pkt->src, tgt->rx_seq);
                                    cdn_list_put(&sock->rx_head, &pkt->node);
                                } else {
                                    d_verbose("rx: l1 no sock\n");
                                    cdn_list_put(&ns->free_pkts, &pkt->node);
                                }
                            } else {
                                d_verbose("rx: l1 seq: %02x != %02x\n", pkt->seq, tgt->rx_seq);
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }
                        } else
#endif
                        { // else
                            d_verbose("rx: l1 no tgt\n");
                            cdn_list_put(&ns->free_pkts, &pkt->node);
                        }
                    } else { // non-seq
                        cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                        if (sock && !sock->tx_only) {
                            cdn_list_put(&sock->rx_head, &pkt->node);
                        } else {
                            d_verbose("rx: l1 no sock\n");
                            cdn_list_put(&ns->free_pkts, &pkt->node);
                        }
                    }
                } else {
                    d_verbose("rx: l1 frame err: %d\n", ret);
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                }

            } else { // rx l2
#ifdef CDN_L2
                // addition in: _l_net; out: seq, l2_frag, l2_uf
                ret = cdn2_from_frame(frame->dat, pkt);

                if (!ret) {
                    if (pkt->src.addr[0] & 0x8) { // seq
                        cdn_tgt_t *tgt = cdn_tgt_search(ns, pkt->src.addr[1], pkt->src.addr[2]);

                        if (tgt) {
                            if ((pkt->seq & 0x7f) == tgt->rx_seq) {
                                tgt->rx_seq = (tgt->rx_seq + 1) & 0x7f;
                                if (pkt->seq & 0x80)
                                    p0_report(ns, pkt->src, tgt->rx_seq);
                                cdn_list_put(&ns->l2_rx, &pkt->node);
                            } else {
                                d_verbose("rx: l2 seq: %02x != %02x\n", pkt->seq, tgt->rx_seq);
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }
                        } else {
                            d_verbose("rx: l2 no tgt\n");
                            cdn_list_put(&ns->free_pkts, &pkt->node);
                        }

                    } else { // non-seq
                        cdn_list_put(&ns->l2_rx, &pkt->node);
                    }
                } else {
                    d_verbose("rx: l2 frame err: %d\n", ret);
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                }
#else
                d_verbose("rx: unknown frame\n");
                cdn_list_put(&ns->free_pkts, &pkt->node);
#endif // CDN_L2
            }

            if (frame)
                dev->put_free_frame(dev, frame);
        }
    }

#ifdef CDN_SEQ
    p0_service_routine(ns);
#endif
}


int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    int retf = -1, ret = 0;
    cdn_intf_t *pre = NULL;

    while (true) { // loop for multi-intf mcast
        cdn_intf_t *intf = cdn_route(ns, pkt, pre);
        if (!intf) {
            if (pre) {
                return ret;
            } else {
                d_verbose("tx: no intf found\n");
                return ret | CDN_RET_ROUTE_ERR;
            }
        }

        cd_frame_t *frame = intf->dev->get_free_frame(intf->dev);
        if (!frame)
            return ret | CDN_RET_NO_FREE;

        if (pkt->src.addr[0] == 0) {
#ifdef CDN_L0_C
            if (pkt->src.port == CDN_DEF_PORT)
                intf->_l0_lp = pkt->dst.port;
#endif
            retf = cdn0_to_frame(pkt, frame->dat);

        } else if (!(pkt->src.addr[0] & 0x40)) {
            retf = cdn1_to_frame(pkt, frame->dat);

        } else {
#ifdef CDN_L2
            retf = cdn2_to_frame(pkt, frame->dat);
#endif
        }

        if (retf) {
            intf->dev->put_free_frame(intf->dev, frame);
            ret |= CDN_RET_FMT_ERR;
        } else {
            intf->dev->put_tx_frame(intf->dev, frame);
        }
        pre = intf;
    }
}


int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    int ret = CDN_RET_FMT_ERR;

    if (pkt->dst.addr[0] == 0) { // l0
        ret = cdn_send_frame(ns, pkt);

    } else if (!(pkt->dst.addr[0] & 0x40) || (pkt->dst.addr[0] & 0xf0) == 0xf0) { // l1
        ret = cdn_send_frame(ns, pkt);

    } else { // l2
#ifdef CDN_L2
        ret = cdn_send_frame(ns, pkt);
#endif
    }

    pkt->ret = 0x80 | ret;
    if (!(pkt->conf & CDN_CONF_NOT_FREE))
        cdn_list_put(&ns->free_pkts, &pkt->node);
    return ret;
}


int cdn_sock_sendto(cdn_sock_t *sock, cdn_pkt_t *pkt)
{
#ifndef CDN_REPLY_KEEP_SEQ
    if (pkt->len && (pkt->dat[0] & 0x80) && (pkt->dst.addr[0] & 0x08))
        pkt->dst.addr[0] &= ~0x08; // clean seq for reply pkt by default
#endif
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

void cdn_init_ns(cdn_ns_t *ns)
{
    memset(ns, 0, sizeof(cdn_ns_t));

#ifdef CDN_SEQ
    ns->sock0.ns = ns;
    ns->sock0.port = 0;
    cdn_sock_bind(&ns->sock0);

    for (int i = 0; i < CDN_TGT_MAX; i++) {
        ns->tgts[i].net = 0xff;
        ns->tgts[i].rx_seq = 0xff;
    }
#endif
}
