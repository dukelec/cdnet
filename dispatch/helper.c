/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdnet_dispatch.h"


cdn_intf_t *cdn_intf_search(cdn_ns_t *ns, uint8_t net, bool route, int *r_idx)
{
    int i;
    if (r_idx)
        *r_idx = -1;

    for (int i = 0; i < CDN_INTF_MAX; i++) {
        if (ns->intfs[i].dev && ns->intfs[i].net == net)
            return &ns->intfs[i];
    }
    if (!route || !CDN_ROUTE_MAX)
        return NULL;

    for (i = 0; i < CDN_ROUTE_MAX; i++) {
        if (ns->routes[i] >> 16 == net) {
            cdn_intf_t *intf = cdn_intf_search(ns, (ns->routes[i] >> 8) & 0xff, false, NULL);
            if (intf) {
                if (r_idx)
                    *r_idx = i;
                return intf;
            }
        }
    }

    if (r_idx)
        *r_idx = 0; // index 0 for default route
    return cdn_intf_search(ns, (ns->routes[0] >> 8) & 0xff, false, NULL);
}

int cdn_mcast_search(cdn_ns_t *ns, uint16_t mid, int start)
{
    for (int i = start; i < CDN_ROUTE_M_MAX; i++) {
        if (ns->routes_m[i] >> 16 == mid) {
            return i;
        }
    }
    return -1;
}

cdn_intf_t *cdn_route(cdn_ns_t *ns, cdn_pkt_t *pkt, int start_idx, int *cur_idx)
{
    cdn_intf_t *intf;

    if ((pkt->dst.addr[0] & 0xf0) != 0xf0 && (pkt->dst.addr[0] & 0xf0) != 0xa0) {
        intf = cdn_intf_search(ns, pkt->dst.addr[1], false, NULL);
        if (!intf)
            return NULL;
        memcpy(pkt->src.addr, pkt->dst.addr, 3);
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        pkt->_d_mac = pkt->dst.addr[2];
        return intf;
    }

    if ((pkt->dst.addr[0] & 0xf0) == 0xa0) {
        int idx;
        intf = cdn_intf_search(ns, pkt->dst.addr[1], true, &idx);
        if (!intf)
            return NULL;
        pkt->src.addr[0] = pkt->dst.addr[0];
        pkt->src.addr[1] = intf->net;
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        pkt->_d_mac = (idx >= 0) ? (ns->routes[idx] & 0xff) : pkt->dst.addr[2];
        return intf;
    }

    { // mcast
        int idx = cdn_mcast_search(ns, pkt->dst.addr[1] << 8 | pkt->dst.addr[2], start_idx);
        if (cur_idx)
            *cur_idx = idx;
        if (idx < 0 && start_idx)
            return NULL;

        if (idx < 0)
            intf = cdn_intf_search(ns, (ns->routes[0] >> 8) & 0xff, false, NULL);
        else
            intf = cdn_intf_search(ns, (ns->routes_m[idx] >> 8) & 0xff, false, NULL);
        if (!intf)
            return NULL;
        pkt->src.addr[0] = (((idx >= 0) && (ns->routes_m[idx] & 0xff)) ? 0xa0 : 0x80) | (pkt->dst.addr[0] & 0x08);
        pkt->src.addr[1] = intf->net;
        pkt->src.addr[2] = intf->mac;
        pkt->_s_mac = intf->mac;
        pkt->_d_mac = pkt->dst.addr[2];
        return intf;
    }
}


#ifdef CDN_RB_TREE
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
#else

cdn_sock_t *cdn_sock_search(cdn_ns_t *ns, uint16_t port)
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

int cdn_sock_insert(cdn_sock_t *sock)
{
    if (cdn_sock_search(sock->ns, sock->port))
        return -1;
    list_put(&sock->ns->socks, &sock->node);
    return 0;
}
#endif // CDN_RB_TREE


#ifdef CDN_SEQ
cdn_tgt_t *cdn_tgt_search(cdn_ns_t *ns, uint8_t net, uint8_t mac)
{
    int i;
    for (i = 0; i < CDN_TGT_MAX; i++) {
        cdn_tgt_t *tgt = &ns->tgts[i];
        if (tgt->net == net && tgt->mac == mac)
            break;
    }

    if (i == CDN_TGT_MAX)
        return NULL;

    if (i != 0) { // move forward
        cdn_tgt_t tmp = ns->tgts[i];
        ns->tgts[i] = ns->tgts[i-1];
        ns->tgts[i-1] = tmp;
        return &ns->tgts[i-1];
    }
    return &ns->tgts[i];
}
#endif
