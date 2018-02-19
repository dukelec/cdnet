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


void cdnet_seq_init(cdnet_intf_t *intf)
{
    int i;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&intf->seq_rx_head);
    list_head_init(&intf->seq_tx_head);
    list_head_init(&intf->seq_tx_direct_head);
#endif

    for (i = 0; i < SEQ_RX_REC_MAX; i++) {
        list_node_t *node = &intf->seq_rx_rec_alloc[i].node;
        seq_rx_rec_t *rec = container_of(node, seq_rx_rec_t, node);
        rec->mac = 255;
        rec->seq_num = 0x80;
#ifdef USE_DYNAMIC_INIT
        rec->is_multi_net = false;
#endif
        list_put(&intf->seq_rx_head, node);
    }

    for (i = 0; i < SEQ_TX_REC_MAX; i++) {
        list_node_t *node = &intf->seq_tx_rec_alloc[i].node;
        seq_tx_rec_t *rec = container_of(node, seq_tx_rec_t, node);
        rec->mac = 255;
        rec->seq_num = 0x80;
#ifdef USE_DYNAMIC_INIT
        rec->is_multi_net = false;
        list_head_init(&rec->wait_head);
        list_head_init(&rec->pend_head);
        rec->pend_cnt = 0;
        rec->send_cnt = 0;
        rec->p0_req = NULL;
        rec->p0_ans = NULL;
        rec->p0_ack = NULL;
#endif
        list_put(&intf->seq_tx_head, node);
    }
}


static bool is_rx_rec_match(const seq_rx_rec_t *rec, const cdnet_packet_t *pkt)
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
static bool is_tx_rec_match_input(const seq_tx_rec_t *rec,
        const cdnet_packet_t *pkt)
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
static bool is_tx_rec_match(const seq_tx_rec_t *rec, const cdnet_packet_t *pkt)
{
    if (pkt->level == CDNET_L1 && pkt->is_multi_net) {
        if (pkt->dst_addr[0] == rec->net && pkt->dst_addr[1] == rec->mac)
            return true;
    } else {
        if (pkt->dst_mac == rec->mac)
            return true;
    }
    return false;
}
static bool is_tx_rec_inuse(const seq_tx_rec_t *rec)
{
    if (rec->wait_head.first || rec->pend_head.first ||
            rec->p0_req || rec->p0_ans || rec->p0_ack)
        return true;
    else
        return false;
}


static int cdnet_send_pkt(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *cd_node;
    cd_frame_t *frame;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val = -1;

    cd_node = cd_intf->get_free_node(cd_intf);
    if (!cd_node) {
        d_warn("cdnet %p: no free frame for tx\n", intf);
        return -1;
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
        return 0;
    } else {
        cd_intf->put_free_node(cd_intf, cd_node);
        d_error("cdnet %p: cdnet_to_frame failed\n", intf);
        return 1;
    }
}

//

static void cdnet_p0_service(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *pre, *cur;
    seq_rx_rec_t *rec = NULL;

    // port 0 service

    list_for_each(&intf->seq_rx_head, pre, cur) {
        seq_rx_rec_t *r = container_of(cur, seq_rx_rec_t, node);
        if (is_rx_rec_match(r, pkt)) {
            rec = r;
            break;
        }
    }

    // in check seq_num
    if (pkt->len == 0) {
        pkt->len = 2;
        pkt->dat[0] = SEQ_TX_PEND_MAX; // TODO: report FREE_SIZE
        pkt->dat[1] = rec ? rec->seq_num : 0x80;
        cdnet_exchg_src_dst(intf, pkt);
        list_put(&intf->seq_tx_direct_head, &pkt->node);
        return;
    }

    // in set seq_num
    if (pkt->len == 2 && pkt->dat[0] == 0x00) {
        if (rec) {
            d_debug("cdnet %p: p0: set seq_num\n", intf);
            rec->seq_num = pkt->dat[1];
            list_move_begin(&intf->seq_rx_head, pre, cur);
        } else {
            list_node_t *n = list_get_last(&intf->seq_rx_head);
            seq_rx_rec_t *r = container_of(n, seq_rx_rec_t, node);
            r->is_multi_net = pkt->is_multi_net;
            if (r->is_multi_net) {
                r->net = pkt->src_addr[0];
                r->mac = pkt->src_addr[1];
            } else {
                r->mac = pkt->src_mac;
            }
            d_debug("cdnet %p: p0: borrow seq_rx\n", intf);
            r->seq_num = pkt->dat[1];
            list_put_begin(&intf->seq_rx_head, n);
        }
        pkt->len = 0;
        cdnet_exchg_src_dst(intf, pkt);
        list_put(&intf->seq_tx_direct_head, &pkt->node);
        return;
    }

    d_warn("cdnet %p: unknown p0 input\n", intf);
    cdnet_list_put(intf->free_head, &pkt->node);
}


void cdnet_p0_request_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *pre, *cur;
    seq_tx_rec_t *rec = NULL;
    (void)(pre); // suppress compiler warning

    // in ack
    if (pkt->len == 3 && pkt->dat[0] == 0x80) {
        list_for_each(&intf->seq_tx_head, pre, cur) {
            seq_tx_rec_t *r = container_of(cur, seq_tx_rec_t, node);
            if (is_tx_rec_match_input(r, pkt)) {
                rec = r;
                break;
            }
        }

        if (!rec || rec->p0_req || rec->p0_ack) {
            d_error("cdnet %p: no match rec for p0 ack\n", intf);
            cdnet_list_put(intf->free_head, &pkt->node);
            return;
        }

        rec->p0_ack = pkt;
        return;
    }

    cdnet_p0_service(intf, pkt);
}

void cdnet_p0_reply_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *pre, *cur;
    seq_tx_rec_t *rec = NULL;
    (void)(pre); // suppress compiler warning

    list_for_each(&intf->seq_tx_head, pre, cur) {
        seq_tx_rec_t *r = container_of(cur, seq_tx_rec_t, node);
        if (is_tx_rec_match_input(r, pkt)) {
            rec = r;
            break;
        }
    }

    if (!rec || !rec->p0_req || rec->p0_ans ||
            (rec->p0_req->len == 0 && pkt->len != 2) ||
            (rec->p0_req->len == 2 && pkt->len != 0)) {
        d_error("cdnet %p: no match rec for p0 answer\n", intf);
        cdnet_list_put(intf->free_head, &pkt->node);
        return;
    }

    rec->p0_ans = pkt;
}

void cdnet_seq_rx_handle(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    list_node_t *pre, *cur;
    seq_rx_rec_t *rec = NULL;

    list_for_each(&intf->seq_rx_head, pre, cur) {
        seq_rx_rec_t *r = container_of(cur, seq_rx_rec_t, node);
        if (is_rx_rec_match(r, pkt)) {
            rec = r;
            break;
        }
    }

    if (!rec || rec->seq_num != pkt->seq_num) {
        d_error("cdnet %p: wrong seq_num, drop\n", intf);
        cdnet_list_put(intf->free_head, &pkt->node);
    } else {
        rec->seq_num = (rec->seq_num + 1) & 0x7f;
        if (pkt->req_ack) {
            list_node_t *nd = cdnet_list_get(intf->free_head);
            if (nd) {
                cdnet_packet_t *p = container_of(nd, cdnet_packet_t, node);
                cdnet_cpy_dst_addr(intf, p, pkt, true);
                p->is_seq = false;
                if (!p->is_multi_net && !p->is_multicast)
                    p->level = CDNET_L0;
                else
                    p->level = CDNET_L1;
                cdnet_fill_src_addr(intf, p);
                p->src_port = CDNET_DEF_PORT;
                p->dst_port = 0;
                p->len = 3;
                p->dat[0] = 0x80;
                p->dat[1] = SEQ_TX_PEND_MAX; // TODO: report FREE_SIZE
                p->dat[2] = rec->seq_num;
                list_put(&intf->seq_tx_direct_head, nd);
                d_verbose("cdnet %p: ret ack %d\n", intf, rec->seq_num);
            } else {
                d_error("cdnet %p: no free pkt (ret ack)\n", intf);
            }
        }
        list_move_begin(&intf->seq_rx_head, pre, cur);
        cdnet_list_put(&intf->rx_head, &pkt->node);
    }
}

void cdnet_seq_tx_task(cdnet_intf_t *intf)
{
    list_node_t     *pre, *cur;
    // TODO: add support for multicast with seq_num:
    //   send pkt directly at first time, then copy the pkt to multiple tx_rec.

    // distribute all items from intf->tx_head to each tx_rec
    while (true) {
        list_node_t *node = cdnet_list_get(&intf->tx_head);
        if (!node)
            break;
        cdnet_packet_t *pkt = container_of(node, cdnet_packet_t, node);
        if (pkt->level == CDNET_L0)
            pkt->is_seq = false;
        if (pkt->is_seq && pkt->dst_mac == 255)
            pkt->is_seq = false; // not support seq_no for multicast yet

        list_for_each(&intf->seq_tx_head, pre, cur) {
            seq_tx_rec_t *r = container_of(cur, seq_tx_rec_t, node);
            if (is_tx_rec_match(r, pkt)) {
                if (pkt->is_seq || is_tx_rec_inuse(r)) {
                    list_put(&r->wait_head, node);
                    node = NULL;
                    list_move_begin(&intf->seq_tx_head, pre, cur);
                }
                break;
            }
        }
        if (!node)
            continue;

        if (!pkt->is_seq) {
            list_put(&intf->seq_tx_direct_head, node);
            continue;
        }

        // pick the oldest rec if not in use
        seq_tx_rec_t *r = container_of(intf->seq_tx_head.last, seq_tx_rec_t, node);
        if (is_tx_rec_inuse(r)) {
            d_warn("cdnet %p: no free tx_rec\n", intf);
            cdnet_list_put_begin(&intf->tx_head, node);
            break;
        }
        list_put_begin(&intf->seq_tx_head, list_get_last(&intf->seq_tx_head));
        r->is_multi_net = pkt->is_multi_net;
        if (r->is_multi_net) {
            r->net = pkt->dst_addr[0];
            r->mac = pkt->dst_addr[1];
        } else {
            r->mac = pkt->dst_mac;
        }
        r->seq_num = 0x80;
        list_head_init(&r->wait_head);
        list_head_init(&r->pend_head);
        r->pend_cnt = 0;
        r->send_cnt = 0;
        list_put(&r->wait_head, node);
    }

    // send packets form seq_tx_direct_head
    list_for_each(&intf->seq_tx_direct_head, pre, cur) {
        cdnet_packet_t *pkt = container_of(cur, cdnet_packet_t, node);
        if (cdnet_send_pkt(intf, pkt) < 0)
            break;
        //d_verbose("cdnet %p: send seq_tx_direct_head ok\n", intf);
        list_get(&intf->seq_tx_direct_head);
        cdnet_list_put(intf->free_head, cur);
        cur = pre;
    }

    list_for_each(&intf->seq_tx_head, pre, cur) {
        list_node_t *p, *c;
        seq_tx_rec_t *r = container_of(cur, seq_tx_rec_t, node);
        if (r->mac == 255)
            break;
        if (r->p0_req && r->p0_ack) {
            cdnet_list_put(intf->free_head, &r->p0_ack->node);
            r->p0_ack = NULL;
        }
        if (!r->p0_req && r->p0_ans) {
            cdnet_list_put(intf->free_head, &r->p0_ans->node);
            r->p0_ans = NULL;
        }
        if (r->p0_req && r->p0_ans) {
            if (r->p0_req->len == 0) { // check return
                // free, as same as get the ack
                list_for_each(&r->pend_head, p, c) {
                    cdnet_packet_t *pkt = container_of(c, cdnet_packet_t, node);
                    if (r->p0_ans->len == 2 && pkt->seq_num == r->p0_ans->dat[1])
                        break;
                    list_get(&r->pend_head);
                    cdnet_list_put(intf->free_head, c);
                    r->pend_cnt--;
                    c = p;
                }
                // re-send left
                if (r->pend_head.first) {
                    d_warn("cdnet %p: re-send pend_head\n", intf);
                    r->seq_num = r->p0_ans->dat[1];
                    r->pend_head.last->next = r->wait_head.first;
                    r->wait_head.first = r->pend_head.first;
                    if (!r->wait_head.last)
                        r->wait_head.last = r->pend_head.last;
                    list_head_init(&r->pend_head);
                }
            } else { // set return
                if (r->pend_head.first) {
                    d_error("cdnet %p: set return: pend not empty\n", intf);
                    list_for_each(&r->pend_head, p, c) {
                        list_get(&r->pend_head);
                        cdnet_list_put(intf->free_head, c);
                        c = p;
                    }
                    r->pend_cnt = 0;
                }
            }

            cdnet_list_put(intf->free_head, &r->p0_req->node);
            cdnet_list_put(intf->free_head, &r->p0_ans->node);
            r->p0_req = NULL;
            r->p0_ans = NULL;
        }
        if (r->p0_ack) {
            list_for_each(&r->pend_head, p, c) {
                cdnet_packet_t *pkt = container_of(c, cdnet_packet_t, node);
                if (pkt->seq_num == r->p0_ack->dat[1])
                    break;
                list_get(&r->pend_head);
                cdnet_list_put(intf->free_head, c);
                r->pend_cnt--;
                c = p;
            }
            cdnet_list_put(intf->free_head, &r->p0_ack->node);
            r->p0_ack = NULL;
        }

        if (r->p0_req) {
            if (get_systick() - r->p0_req->send_time > SEQ_TIMEOUT) {
                d_warn("cdnet %p: p0 req timeout, len: %d\n",
                        intf, r->p0_req->len);
                // TODO: limit retry counts
                if (cdnet_send_pkt(intf, r->p0_req) == 0)
                    r->p0_req->send_time = get_systick();
            }
            continue;
        }

        if (r->seq_num & 0x80) {
            list_node_t *node = cdnet_list_get(intf->free_head);
            if (!node) {
                d_error("cdnet %p: no free pkt (set seq_no)\n", intf);
                continue;
            }
            r->seq_num = 0;
            r->p0_req = container_of(node, cdnet_packet_t, node);
            r->p0_req->level = CDNET_L0; // TODO: add multi_net support
            r->p0_req->dst_mac = r->mac;
            cdnet_fill_src_addr(intf, r->p0_req);
            r->p0_req->src_port = CDNET_DEF_PORT;
            r->p0_req->dst_port = 0;
            r->p0_req->len = 2;
            r->p0_req->dat[0] = 0x00;
            r->p0_req->dat[1] = 0x00;
            if (cdnet_send_pkt(intf, r->p0_req) == 0)
                r->p0_req->send_time = get_systick();
            else
                r->p0_req->send_time = get_systick() - SEQ_TIMEOUT;
            d_debug("cdnet %p: set seq_num\n", intf);
            continue;
        }

        // check if pend timeout
        if (r->pend_head.first) {
            cdnet_packet_t *pkt = container_of(r->pend_head.first, cdnet_packet_t, node);
            if (get_systick() - pkt->send_time > SEQ_TIMEOUT) {
                d_warn("cdnet %p: pending timeout\n", intf);
                // send check
                list_node_t *node = cdnet_list_get(intf->free_head);
                if (!node) {
                    d_error("cdnet %p: no free pkt (check seq_no)\n", intf);
                    continue;
                }
                r->p0_req = container_of(node, cdnet_packet_t, node);
                r->p0_req->level = CDNET_L0; // TODO: add multi_net support
                r->p0_req->dst_mac = r->mac;
                cdnet_fill_src_addr(intf, r->p0_req);
                r->p0_req->src_port = CDNET_DEF_PORT;
                r->p0_req->dst_port = 0;
                r->p0_req->len = 0;
                if (cdnet_send_pkt(intf, r->p0_req) == 0)
                    r->p0_req->send_time = get_systick();
                else
                    r->p0_req->send_time = get_systick() - SEQ_TIMEOUT;
                r->send_cnt = 0;
            }
        }

        // send wait_head
        list_for_each(&r->wait_head, p, c) {
            int ret;
            cdnet_packet_t *pkt = container_of(c, cdnet_packet_t, node);

            if (r->pend_cnt > SEQ_TX_PEND_MAX)
                break;
            if (pkt->is_seq) {
                pkt->seq_num = r->seq_num;
                if (++r->send_cnt == SEQ_TX_ACK_CNT) {
                    r->send_cnt = 0;
                    pkt->req_ack = true;
                } else {
                    pkt->req_ack = false;
                }
            }
            ret = cdnet_send_pkt(intf, pkt);
            if (ret < 0)
                break;
            list_get(&r->wait_head);
            if (ret == 0 && pkt->is_seq) {
                r->seq_num = (r->seq_num + 1) & 0x7f;
                pkt->send_time = get_systick();
                list_put(&r->pend_head, c);
                r->pend_cnt++;
            } else {
                if (ret != 0)
                    d_error("cdnet %p: send wait_head error\n", intf);
                cdnet_list_put(intf->free_head, c);
            }
            c = p;
        }
    }
}
