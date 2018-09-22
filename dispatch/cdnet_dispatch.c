/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdnet_dispatch.h"


list_head_t cdnet_free_pkts = {0};

// TODO: add multiple interface support
cdnet_intf_t *cdnet_intfs[1] = {0};

rb_root_t cdnet_sockets = RB_ROOT;


cdnet_intf_t *cdnet_intf_search(uint8_t net)
{
    return cdnet_intfs[0];
}

cdnet_intf_t *cdnet_route_search(const cd_addr_t *d_addr, uint8_t *d_mac)
{
    if (d_mac)
        *d_mac = d_addr->cd_addr_mac;
    return cdnet_intfs[0];
}


cdnet_socket_t *cdnet_socket_search(uint16_t port)
{
    struct rb_root *root = &cdnet_sockets;
    struct rb_node *node = root->rb_node;

    while (node) {
        cdnet_socket_t *sock = container_of(node, cdnet_socket_t, node);

        if (port < sock->port)
            node = node->rb_left;
        else if (port > sock->port)
            node = node->rb_right;
        else
            return sock;
    }
    return NULL;
}

int cdnet_socket_insert(cdnet_socket_t *sock)
{
    struct rb_root *root = &cdnet_sockets;
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while (*new) {
        cdnet_socket_t *this = container_of(*new, cdnet_socket_t, node);

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


int cdnet_intf_register(cdnet_intf_t *intf)
{
    if (cdnet_intfs[0] == NULL)
        cdnet_intfs[0] = intf;
    else
        return -1;
    return 0;
}

void cdnet_intf_init(cdnet_intf_t *intf, cd_dev_t *dev,
        uint8_t net, uint8_t mac)
{
    intf->dev = dev;
    intf->net = net;
    intf->mac = mac;
}

int cdnet_intf_sendto(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    int ret = 0;

    cd_frame_t *frame = intf->dev->get_free_frame(intf->dev);
    if (!frame) {
        ret = -1;
        goto no_frame_out;
    }

    ret = cdnet_l1_to_frame(&pkt->src, &pkt->dst,
            pkt->dat, pkt->len, intf->mac, 0, frame->dat);
    if (ret)
        intf->dev->put_free_frame(intf->dev, frame);
    else
        intf->dev->put_tx_frame(intf->dev, frame);

no_frame_out:
    cdnet_list_put(&cdnet_free_pkts, &pkt->node);
    return ret;
}


void cdnet_intf_routine(void)
{
    cdnet_intf_t *intf = cdnet_intfs[0];
    cd_frame_t *frame;
    cdnet_packet_t *pkt;
    cd_dev_t *dev = intf->dev;
    int ret;

    while (true) {
        if (!cdnet_free_pkts.len) {
            d_warn("rx: no free pkt\n");
            return;
        }

        frame = dev->get_rx_frame(dev);
        if (!frame)
            return;
        pkt = cdnet_packet_get(&cdnet_free_pkts);

        if ((frame->dat[3] & 0xc0) == 0xc0) {
            ret = -1;
        } else if (frame->dat[3] & 0x80) {
            ret = cdnet_l1_from_frame(frame->dat, intf->net,
                    &pkt->src, &pkt->dst, pkt->dat, &pkt->len, NULL);
        } else {
            ret = -1;
        }

        dev->put_free_frame(dev, frame);

        if (ret != 0) {
            d_error("rx: from_frame err\n");
            goto error;
        }

        cdnet_socket_t *sock = cdnet_socket_search(pkt->dst.port);

        if (!sock) {
            d_error("rx: sock port %d not found\n", pkt->dst.port);
            goto error;
        }

        cdnet_list_put(&sock->rx_head, &pkt->node);
        continue;
error:
        cdnet_list_put(&cdnet_free_pkts, &pkt->node);
    }
}


int cdnet_socket_bind(cdnet_socket_t *sock, cd_sockaddr_t *addr)
{
    if (addr)
        sock->port = addr->port;
    return cdnet_socket_insert(sock);
}

int cdnet_socket_sendto(cdnet_socket_t *sock, cdnet_packet_t *pkt)
{
    int ret;
    cdnet_intf_t *intf = cdnet_intfs[0];
    pkt->src.addr.cd_addr = pkt->dst.addr.cd_addr;
    pkt->src.addr.cd_addr_mac = intf->mac;
    pkt->src.port = sock->port;

    ret = cdnet_intf_sendto(intf, pkt);
    return ret;
}

cdnet_packet_t *cdnet_socket_recvfrom(cdnet_socket_t *sock)
{
    if (!sock->rx_head.len)
        return NULL;
    return cdnet_packet_get(&sock->rx_head);
}
