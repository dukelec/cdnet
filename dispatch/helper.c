/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet_dispatch.h"


cdn_intf_t *cdn_intf_search(cdn_ns_t *ns, uint8_t net)
{
    for (int i = 0; i < CDN_INTF_MAX; i++) {
        if (ns->intfs[i].dev && ns->intfs[i].net == net)
            return &ns->intfs[i];
    }
    return NULL;
}

cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    cdn_intf_t *intf;

    if ((pkt->dst->addr[0] & 0xf0) != 0xf0 && (pkt->dst->addr[0] & 0xf0) != 0xa0) {
        intf = cdn_intf_search(ns, pkt->dst->addr[1]);
        if (!intf)
            return NULL;
        memcpy(pkt->src.addr, pkt->dst.addr, 3);
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        pkt->_d_mac = pkt->dst->addr[2];
        return intf;
    }

    if ((pkt->dst->addr[0] & 0xf0) == 0xa0) {
        intf = cdn_intf_search(ns, pkt->dst->addr[1]);
        if (!intf) {
            int i;
            for (i = 0; i < CDN_ROUTE_MAX; i++) {
                if (ns->route[i] >> 16 == pkt->dst->addr[1])
                    break;
            }
            if (i == CDN_ROUTE_MAX)
                i = 0; // use default gateway
            intf = cdn_intf_search(ns, (ns->route[i] >> 8) & 0xff);
            pkt->_d_mac = ns->route[i] & 0xff;
        } else {
            pkt->_d_mac = pkt->dst->addr[2];
        }
        pkt->src.addr[0] = pkt->dst.addr[0];
        pkt->src.addr[1] = intf->net;
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        return intf;
    }

    { // mcast
        uint8_t net;
#ifdef CDN_TGT
        cdn_tgt_t *tgt = cdn_tgt_search(ns, pkt->dst->addr[1] << 8 | pkt->dst->addr[2], NULL);
        if (tgt && tgt->tgts.len) {
            cdn_tgt_t *st = container_of(tgt->tgts.first, cdn_tgt_t, node);
            net = st->id >> 8;
        } else
#endif
        { // else
            net = ns->intfs[0].net; // default interface
        }

        intf = cdn_intf_search(ns, net);
        if (!intf) {
            int i;
            for (i = 0; i < CDN_ROUTE_MAX; i++) {
                if (ns->route[i] >> 16 == net)
                    break;
            }
            if (i == CDN_ROUTE_MAX)
                i = 0; // use default gateway
            intf = cdn_intf_search(ns, (ns->route[i] >> 8) & 0xff);
            pkt->src.addr[0] = (pkt->dst.addr[0] & 0x08) ? 0xa8 : 0xa0;
            pkt->_d_mac = ns->route[i] & 0xff;
        } else {
            pkt->src.addr[0] = (pkt->dst.addr[0] & 0x08) ? 0x88 : 0x80;
            pkt->_d_mac = pkt->dst->addr[2];
        }
        pkt->src.addr[1] = intf->net;
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        return intf;
    }
}


cdn_sock_t *cdn_sock_search(cdn_ns_t *ns, uint16_t port)
{
    struct rb_root *root = &ns->socks;
    struct rb_node *node = root->rb_node;

    while (node) {
        cdn_sock_t *sock = container_of(node, cdn_sock_t, node);

        if (port < sock->port)
            node = node->rb_left;
        else if (port > sock->port)
            node = node->rb_right;
        else
            return sock;
    }
    return NULL;
}

int cdn_sock_insert(cdn_sock_t *sock)
{
    struct rb_root *root = &sock->ns->socks;
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while (*new) {
        cdn_sock_t *this = container_of(*new, cdn_sock_t, node);

        parent = *new;
        if (sock->port < this->port)
            new = &((*new)->rb_left);
        else if (sock->port > this->port)
            new = &((*new)->rb_right);
        else
            return -1;
    }

    // add new node and rebalance tree
    rb_link_node(&sock->node, parent, new);
    rb_insert_color(&sock->node, root);
    return 0;
}

#ifdef CDN_TGT
cdn_tgt_t *cdn_tgt_search(cdn_ns_t *ns, uint16_t id, cdn_tgt_t **parent)
{
    list_node_t *pre, *pos, *spre, *spos;
    list_for_each(ns->tgts, pre, pos) {
        cdn_tgt_t *t = container_of(pos, cdn_tgt_t, node);
        if (t->id == id) {
            if (parent)
                *parent = NULL;
            return t;
        }
        list_for_each(t->tgts, spre, spos) {
            cdn_tgt_t *st = container_of(spos, cdn_tgt_t, node);
            if (st->id == id) {
                if (parent)
                    *parent = t;
                return st;
            }
        }
    }
    return NULL;
}
#endif
