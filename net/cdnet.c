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


int cdnet_l0_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf);
int cdnet_l1_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf);
int cdnet_l2_to_frame(cdnet_intf_t *intf, cdnet_packet_t *pkt, uint8_t *buf);

int cdnet_l0_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);
int cdnet_l1_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);
int cdnet_l2_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt);


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
        cd_intf_t *cd_intf, uint8_t mac)
{
    int i;

    intf->mac = mac; // 255: unspecified
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->net = 0;
    intf->l0_last_port = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    list_head_init(&intf->seq_free_head);
    list_head_init(&intf->seq_rx_head);
    list_head_init(&intf->seq_tx_head);
#endif

    for (i = 0; i < SEQ_REC_MAX; i++) {
        list_node_t *node = &intf->seq_rec_alloc[i].node;
        seq_rec_t *rec = container_of(node, seq_rec_t, node);
        rec->mac = 255;
        rec->seq_num = 0x80;
#ifdef USE_DYNAMIC_INIT
        rec->is_multi_net = false;
        list_head_init(&rec->pend_head);
        rec->pend_cnt = 0;
        rec->send_cnt = 0;
        rec->p1_req = NULL;
#endif
        if (i < SEQ_REC_MAX / 2)
            list_put(&intf->seq_rx_head, node);
        else
            list_put(&intf->seq_tx_head, node);
    }
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


static bool is_rec_match(const seq_rec_t *rec, const cdnet_packet_t *pkt)
{
    if (pkt->level == CDNET_L1 && pkt->is_multi_net) {
        if (pkt->src_addr[0] == rec->net && pkt->src_addr[1] == rec->mac)
            return true;
    } else {
        if (pkt->src_mac == rec->mac)
            return true;
    }
    return false;
}

static void cdnet_send_pkt(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *cd_node;
    cd_frame_t *frame;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val = -1;

    cd_node = cd_intf->get_free_node(cd_intf);
    if (!cd_node) {
        d_error("cdnet %p: no free frame for tx\n", intf);
        return;
    }
    frame = container_of(cd_node, cd_frame_t, node);

    if (pkt->level == CDNET_L0)
        ret_val = cdnet_l0_to_frame(intf, pkt, frame->dat);
    else if (pkt->level == CDNET_L1)
        ret_val = cdnet_l1_to_frame(intf, pkt, frame->dat);
    else if (pkt->level == CDNET_L2)
        ret_val = cdnet_l2_to_frame(intf, pkt, frame->dat);

    if (ret_val == 0) {
        cd_intf->put_tx_node(cd_intf, cd_node);
    } else {
        cd_intf->put_free_node(cd_intf, cd_node);
        d_error("cdnet %p: cdnet_to_frame failed\n", intf);
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

    net_node = list_get(intf->free_head);
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
        list_put(intf->free_head, net_node);
        return;
    }

    if (pkt->level != CDNET_L2 && pkt->dst_port == 1 &&
            pkt->src_port >= CDNET_DEF_PORT) {
        list_node_t *pre, *cur;
        seq_rec_t *rec = NULL;

        // in ack

        if (pkt->len == 3 && pkt->dat[0] == 0x80) {
            list_for_each(&intf->seq_tx_head, pre, cur) {
                seq_rec_t *r = container_of(cur, seq_rec_t, node);
                if (is_rec_match(r, pkt)) {
                    rec = r;
                    break;
                }
            }
            if (rec) {
                list_for_each(&rec->pend_head, pre, cur) {
                    cdnet_packet_t *p;
                    p = container_of(cur, cdnet_packet_t, node);
                    list_pick(&rec->pend_head, pre, cur);
                    list_put(intf->free_head, cur);
                    rec->pend_cnt--;
                    if (p->seq_num == pkt->dat[2])
                        break;
                    cur = pre;
                }
            }
            list_put(intf->free_head, net_node);
            return;
        }

        // port 1 service

        rec = NULL;
        list_for_each(&intf->seq_rx_head, pre, cur) {
            seq_rec_t *r = container_of(cur, seq_rec_t, node);
            if (is_rec_match(r, pkt)) {
                rec = r;
                break;
            }
        }

        // in check seq_num
        if (pkt->len == 0) {
            pkt->len = 2;
            pkt->dat[0] = 20; // TODO: report FREE_SIZE
            pkt->dat[1] = rec ? rec->seq_num : 0x80;
            cdnet_exchg_src_dst(intf, pkt);
            cdnet_send_pkt(intf, pkt);
            list_put(intf->free_head, net_node);
            return;
        }

        // in set seq_num
        if (pkt->len == 2 && pkt->dat[0] == 0x00) {
            if (rec) {
                d_debug("cdnet %p: port1: set seq_num\n", intf);
                rec->seq_num = pkt->dat[1];
                list_move_begin(&intf->seq_rx_head, pre, cur);
            } else {
                list_node_t *n = list_get_last(&intf->seq_rx_head);
                if (!n) {
                    d_error("cdnet %p: port1: no rx seq node\n", intf);
                    list_put(intf->free_head, net_node);
                    return;
                }
                seq_rec_t *r = container_of(n, seq_rec_t, node);
                r->is_multi_net = pkt->is_multi_net;
                if (r->is_multi_net) {
                    r->net = pkt->src_addr[0];
                    r->mac = pkt->src_addr[1];
                } else {
                    r->mac = pkt->src_mac;
                }
                d_debug("cdnet %p: port1: add seq_num\n", intf);
                r->seq_num = pkt->dat[1];
                list_put_begin(&intf->seq_rx_head, n);
            }
            pkt->dat[0] = 20; // TODO: report FREE_SIZE
            cdnet_exchg_src_dst(intf, pkt);
            cdnet_send_pkt(intf, pkt);
            list_put(intf->free_head, net_node);
            return;
        }

        d_warn("cdnet %p: unknown p1 input\n", intf);
        list_put(intf->free_head, net_node);
        return;
    }

    // port 1 return
    if (pkt->level != CDNET_L2 && pkt->src_port == 1 &&
            pkt->dst_port == CDNET_DEF_PORT) {
        list_node_t *pre, *cur;
        seq_rec_t *rec = NULL;

        list_for_each(&intf->seq_tx_head, pre, cur) {
            seq_rec_t *r = container_of(cur, seq_rec_t, node);
            if (is_rec_match(r, pkt)) {
                rec = r;
                break;
            }
        }

        if (!rec || !rec->p1_req || pkt->len != 2) {
            d_error("cdnet %p: no match rec for p1 answer\n", intf);
            list_put(intf->free_head, net_node);
            return;
        }

        if (pkt->dat[1] & 0x80) { // no seq record
            d_error("cdnet %p: p1 answer no seq record\n", intf);
            // move all pending to intf->tx_head
            if (rec->pend_head.last) {
                rec->pend_head.last->next = intf->tx_head.first;
                intf->tx_head.first = rec->pend_head.first;
                if (!intf->tx_head.last)
                    intf->tx_head.last = rec->pend_head.last;
            }
            // de-init rec
            rec->seq_num = 0x80;
            list_put(intf->free_head, net_node);
            return;
        }

        if (rec->p1_req->len == 0) { // check return
            // free, as same as get the ack
            list_for_each(&rec->pend_head, pre, cur) {
                cdnet_packet_t *p;
                p = container_of(cur, cdnet_packet_t, node);
                list_pick(&rec->pend_head, pre, cur);
                list_put(intf->free_head, cur);
                rec->pend_cnt--;
                if (p->seq_num == pkt->dat[1])
                    break;
                cur = pre;
            }
            // re-send left
            list_for_each(&rec->pend_head, pre, cur) {
                cdnet_packet_t *p;
                p = container_of(cur, cdnet_packet_t, node);
                cdnet_send_pkt(intf, p);
                p->send_time = get_systick();
            }
        } // else do nothing for the set return

        // free p1_req
        list_put(intf->free_head, &rec->p1_req->node);
        rec->p1_req = NULL;
        list_put(intf->free_head, net_node);
        return;
    }

    // check come in packet seq_num
    if (pkt->level != CDNET_L0 && pkt->is_seq) {
        list_node_t *pre, *cur;
        seq_rec_t *rec = NULL;

        list_for_each(&intf->seq_rx_head, pre, cur) {
            seq_rec_t *r = container_of(cur, seq_rec_t, node);
            if (is_rec_match(r, pkt)) {
                rec = r;
                break;
            }
        }

        if (!rec || rec->seq_num != pkt->seq_num) {
            d_error("cdnet %p: wrong seq_num, drop\n", intf);
            list_put(intf->free_head, net_node);
            return;
        } else {
            rec->seq_num++;
            rec->seq_num &= 0x7f;
            list_move_begin(&intf->seq_rx_head, pre, cur);
        }
    }

    // send pkt to upper layer
    list_put(&intf->rx_head, net_node);
}

void cdnet_tx(cdnet_intf_t *intf)
{
    // TODO: distribute all items from intf->tx_head to each seq_rec first
    list_node_t     *pre, *cur;
    cdnet_packet_t  *np = NULL;         // new packet for send
    seq_rec_t       *np_r = NULL;       // new packet corresponding seq_rec
    list_node_t     *empty_p = NULL;    // pre node of the empty seq_rec
    seq_rec_t       *empty_r = NULL;

    if (intf->tx_head.first)
        np = container_of(intf->tx_head.first, cdnet_packet_t, node);

    list_for_each(&intf->seq_tx_head, pre, cur) {
        seq_rec_t *r = container_of(cur, seq_rec_t, node);

        if (np && is_rec_match(r, np))
            np_r = r;
        if (!r->p1_req && !r->pend_head.first) {
            empty_p = pre;
            empty_r = r;
        }

        if (r->p1_req) {
            if (get_systick() - r->p1_req->send_time > SEQ_TIMEOUT) {
                d_warn("cdnet %p: p1 req timeout\n", intf);
                cdnet_send_pkt(intf, r->p1_req);
                r->p1_req->send_time = get_systick();
            }
            continue;
        }

        if (r->pend_head.first) {
            cdnet_packet_t *p;
            p = container_of(r->pend_head.first, cdnet_packet_t, node);
            if (get_systick() - p->send_time > SEQ_TIMEOUT) {
                d_warn("cdnet %p: pending timeout\n", intf);
                // send check
                list_node_t *n = list_get(intf->free_head);
                if (!n) {
                    d_error("cdnet %p: no free pkt\n", intf);
                    continue;
                }
                r->p1_req = container_of(n, cdnet_packet_t, node);

                // TODO: add multi_net support
                r->p1_req->level = CDNET_L0;
                r->p1_req->dst_mac = r->mac;
                cdnet_fill_src_addr(intf, r->p1_req);
                r->p1_req->src_port = CDNET_DEF_PORT;
                r->p1_req->dst_port = 1;
                r->p1_req->len = 0;
                cdnet_send_pkt(intf, p);
                r->p1_req->send_time = get_systick();
                r->send_cnt = 0;
            }
        }
    }

    if (!np) // no pkt for tx
        return;

    // send pkt which not use seq_num
    if ((np->level == CDNET_L0 || !np->is_seq) &&
            (!np_r || (!np_r->p1_req && !np_r->pend_head.first))) {
        cdnet_send_pkt(intf, np);
        list_get(&intf->tx_head);
        list_put(intf->free_head, &np->node);
        return;
    }

    if (!np_r || (np_r->seq_num & 0x80)) { // no corresponding seq_rec
        if (!intf->free_head->first) {
            d_error("cdnet %p: no free pkt (set seq)\n", intf);
            return;
        }

        if (!np_r) {
            if (empty_r) {
                list_pick(&intf->seq_tx_head, empty_p, &empty_r->node);
                np_r = empty_r;
            } else {
                d_error("cdnet %p: no free rec node\n", intf);
                return;
            }
        }
        list_put_begin(&intf->seq_tx_head, &np_r->node);

        np_r->is_multi_net = np->is_multi_net;
        if (np->is_multi_net) {
            np_r->net = np->src_addr[0];
            np_r->mac = np->src_addr[1];
        } else {
            np_r->mac = np->src_mac;
        }
        np_r->seq_num = 0;
        list_head_init(&np_r->pend_head);
        np_r->pend_cnt = 0;
        np_r->send_cnt = 0;

        // send set, TODO: add multi_net support
        np_r->p1_req = container_of(list_get(intf->free_head),
                cdnet_packet_t, node);
        np_r->p1_req->level = CDNET_L0;
        np_r->p1_req->dst_mac = np_r->mac;
        cdnet_fill_src_addr(intf, np_r->p1_req);
        np_r->p1_req->src_port = CDNET_DEF_PORT;
        np_r->p1_req->dst_port = 1;
        np_r->p1_req->len = 2;
        np_r->p1_req->dat[0] = 0x00;
        np_r->p1_req->dat[1] = 0x00;
        cdnet_send_pkt(intf, np_r->p1_req);
        np_r->p1_req->send_time = get_systick();
        d_debug("cdnet %p: set seq_num\n", intf);
        return;
    }

    // send pkt which use seq_num
    if (np_r->p1_req || np_r->pend_cnt > 10)
        return;
    if (++np_r->send_cnt == 5) {
        np_r->send_cnt = 0;
        np->req_ack = true;
    } else {
        np->req_ack = false;
    }
    np->seq_num = np_r->seq_num++;
    np_r->seq_num &= 0x7f;
    cdnet_send_pkt(intf, np);
    np->send_time = get_systick();
    list_get(&intf->tx_head);
    list_put(&np_r->pend_head, &np->node);
    np_r->pend_cnt++;
}
