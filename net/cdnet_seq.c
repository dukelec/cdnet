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
        seq_rx_rec_t *rec = list_entry(node, seq_rx_rec_t);
        rec->addr.net = 255;
        rec->addr.mac = 255;
        rec->seq_num = 0x80;
        list_put(&intf->seq_rx_head, node);
    }

    for (i = 0; i < SEQ_TX_REC_MAX; i++) {
        list_node_t *node = &intf->seq_tx_rec_alloc[i].node;
        seq_tx_rec_t *rec = list_entry(node, seq_tx_rec_t);
        rec->addr.net = 255;
        rec->addr.mac = 255;
        rec->seq_num = 0x80;
#ifdef USE_DYNAMIC_INIT
        list_head_init(&rec->wait_head);
        list_head_init(&rec->pend_head);
        rec->send_cnt = 0;
        rec->p0_retry_cnt = 0;
        rec->p0_req = NULL;
        rec->p0_ans = NULL;
        rec->p0_ack = NULL;
#endif
        list_put(&intf->seq_tx_head, node);
    }
}


static bool is_rx_rec_match(const seq_rx_rec_t *rec, const cdnet_packet_t *pkt)
{
    if (pkt->level == CDNET_L1 && pkt->multi >= CDNET_MULTI_NET) {
        if (is_addr_equal(&pkt->src_addr, &rec->addr))
            return true;
    } else {
        if (pkt->src_mac == rec->addr.mac && rec->addr.net == 255)
            return true;
    }
    return false;
}
static bool is_tx_rec_match_input(const seq_tx_rec_t *rec,
        const cdnet_packet_t *pkt)
{
    if (pkt->level == CDNET_L1 && pkt->multi >= CDNET_MULTI_NET) {
        if (is_addr_equal(&pkt->src_addr, &rec->addr))
            return true;
    } else {
        if (pkt->src_mac == rec->addr.mac && rec->addr.net == 255)
            return true;
    }
    return false;
}
static bool is_tx_rec_match(const seq_tx_rec_t *rec, const cdnet_packet_t *pkt)
{
    if (pkt->level == CDNET_L1 && pkt->multi >= CDNET_MULTI_NET) {
        if (is_addr_equal(&pkt->dst_addr, &rec->addr))
            return true;
    } else {
        if (pkt->dst_mac == rec->addr.mac && rec->addr.net == 255)
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
    cd_frame_t *frame;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val = -1;

    frame = cd_intf->get_free_frame(cd_intf);
    if (!frame) {
        dd_warn(intf->name, "tx: no free frame\n");
        return -1;
    }

    if (pkt->level == CDNET_L0)
        ret_val = cdnet_l0_to_frame(intf, pkt, frame->dat);
    else if (pkt->level == CDNET_L1)
        ret_val = cdnet_l1_to_frame(intf, pkt, frame->dat);
    else if (pkt->level == CDNET_L2)
        ret_val = cdnet_l2_to_frame(intf, pkt, frame->dat);

    if (ret_val == 0) {
        cd_intf->put_tx_frame(cd_intf, frame);
        return 0;
    } else {
        cd_intf->put_free_frame(cd_intf, frame);
        dd_error(intf->name, "tx: to_frame err\n");
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
        seq_rx_rec_t *r = list_entry(cur, seq_rx_rec_t);
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
            rec->seq_num = pkt->dat[1];
            dd_debug(intf->name, "p0_rx: set seq rec: %d\n", rec->seq_num);
            list_move_begin(&intf->seq_rx_head, pre, cur);
        } else {
            seq_rx_rec_t *r;
            r = list_entry(list_get_last(&intf->seq_rx_head), seq_rx_rec_t);
            if (pkt->multi == CDNET_MULTI_NET) {
                r->addr = pkt->src_addr;
            } else {
                r->addr.mac = pkt->src_mac;
                r->addr.net = 255;
            }
            r->seq_num = pkt->dat[1];
            dd_debug(intf->name, "p0_rx: pick seq rec: %d\n", r->seq_num);
            list_put_begin(&intf->seq_rx_head, &r->node);
        }
        pkt->len = 0;
        cdnet_exchg_src_dst(intf, pkt);
        list_put(&intf->seq_tx_direct_head, &pkt->node);
        return;
    }

    dd_warn(intf->name, "p0_rx: unknown pkt\n");
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
            seq_tx_rec_t *r = list_entry(cur, seq_tx_rec_t);
            if (is_tx_rec_match_input(r, pkt)) {
                rec = r;
                break;
            }
        }

        if (!rec || rec->p0_req || rec->p0_ack) {
            dd_error(intf->name, "p0_rx: no rec found for ack\n");
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
        seq_tx_rec_t *r = list_entry(cur, seq_tx_rec_t);
        if (is_tx_rec_match_input(r, pkt)) {
            rec = r;
            break;
        }
    }

    if (!rec || !rec->p0_req || rec->p0_ans ||
            (rec->p0_req->len == 0 && pkt->len != 2) ||
            (rec->p0_req->len == 2 && pkt->len != 0)) {
        dd_error(intf->name, "p0_rx: no rec found for ans\n");
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
        seq_rx_rec_t *r = list_entry(cur, seq_rx_rec_t);
        if (is_rx_rec_match(r, pkt)) {
            rec = r;
            break;
        }
    }

    if (!rec || rec->seq_num != pkt->_seq_num) {
        dd_error(intf->name, "seq_rx: wrong seq, r: %d, i: %d\n",
                rec->seq_num, pkt->_seq_num);
        cdnet_list_put(intf->free_head, &pkt->node);
    } else {
        rec->seq_num = (rec->seq_num + 1) & 0x7f;
        if (pkt->_req_ack) {
            cdnet_packet_t *p = cdnet_packet_get(intf->free_head);
            if (p) {
                memcpy(p, pkt, offsetof(cdnet_packet_t, src_port));
                cdnet_exchg_src_dst(intf, p);
                p->seq = false;
                if (p->multi)
                    p->level = CDNET_L1;
                else
                    p->level = CDNET_L0;
                p->src_port = CDNET_DEF_PORT;
                p->dst_port = 0;
                p->len = 3;
                p->dat[0] = 0x80;
                p->dat[1] = SEQ_TX_PEND_MAX; // TODO: report FREE_SIZE
                p->dat[2] = rec->seq_num;
                list_put(&intf->seq_tx_direct_head, &p->node);
                dd_verbose(intf->name, "seq_rx: ret ack: %d\n", rec->seq_num);
            } else {
                dd_error(intf->name, "seq_rx: ret ack: no free pkt\n");
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
        seq_tx_rec_t *rec = NULL;
        cdnet_packet_t *pkt = cdnet_packet_get(&intf->tx_head);
        if (!pkt)
            break;
        if (pkt->level == CDNET_L0)
            pkt->seq = false;
        if (pkt->level != CDNET_L1)
            pkt->multi = CDNET_MULTI_NONE;
        if (pkt->seq && pkt->dst_mac == 255) {
            pkt->seq = false;
            dd_warn(intf->name, "tx: not support seq for broadcast yet\n");
        }

        list_for_each(&intf->seq_tx_head, pre, cur) {
            seq_tx_rec_t *r = list_entry(cur, seq_tx_rec_t);
            if (is_tx_rec_match(r, pkt)) {
                rec = r;
                if (pkt->seq || is_tx_rec_inuse(r)) {
                    list_put(&r->wait_head, &pkt->node);
                    pkt = NULL;
                    list_move_begin(&intf->seq_tx_head, pre, cur);
                }
                break;
            }
        }
        if (!pkt)
            continue;
        if (!pkt->seq) {
            list_put(&intf->seq_tx_direct_head, &pkt->node);
            continue;
        }
        if (!rec)  {
            // pick the oldest rec if not in use
            rec = list_entry(intf->seq_tx_head.last, seq_tx_rec_t);
            if (is_tx_rec_inuse(rec)) {
                dd_warn(intf->name, "tx: no free rec\n");
                cdnet_list_put_begin(&intf->tx_head, &pkt->node);
                break;
            }
            list_node_t *node = list_get_last(&intf->seq_tx_head);
            memset(rec, 0, sizeof(*rec));
            list_put_begin(&intf->seq_tx_head, node);
            if (pkt->multi) {
                rec->addr = pkt->dst_addr;
            } else {
                rec->addr.mac = pkt->dst_mac;
                rec->addr.net = 255;
            }
            rec->seq_num = 0x80;
            dd_debug(intf->name, "tx: pick seq rec\n");
        }
        list_put(&rec->wait_head, &pkt->node);
    }

    // send packets form seq_tx_direct_head
    list_for_each(&intf->seq_tx_direct_head, pre, cur) {
        cdnet_packet_t *pkt = list_entry(cur, cdnet_packet_t);
        if (cdnet_send_pkt(intf, pkt) < 0)
            break;
        //dd_verbose(intf->name, "tx: mv to direct_head\n");
        list_get(&intf->seq_tx_direct_head);
        cdnet_list_put(intf->free_head, cur);
        cur = pre;
    }

    list_for_each(&intf->seq_tx_head, pre, cur) {
        list_node_t *p, *c;
        seq_tx_rec_t *r = list_entry(cur, seq_tx_rec_t);
        if (r->addr.mac == 255)
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
                    cdnet_packet_t *pkt = list_entry(c, cdnet_packet_t);
                    if (r->p0_ans->len == 2 && pkt->_seq_num == r->p0_ans->dat[1])
                        break;
                    list_get(&r->pend_head);
                    cdnet_list_put(intf->free_head, c);
                    c = p;
                }
                // re-send left
                if (r->pend_head.first) {
                    dd_warn(intf->name, "tx: chk_seq ret: re-send pend_head\n");
                    r->seq_num = r->p0_ans->dat[1];
                    r->pend_head.last->next = r->wait_head.first;
                    r->wait_head.first = r->pend_head.first;
                    if (!r->wait_head.last)
                        r->wait_head.last = r->pend_head.last;
                    list_head_init(&r->pend_head);
                }
            } else { // set return
                if (r->pend_head.first) {
                    dd_error(intf->name, "tx: set_seq ret: pend_head not empty\n");
                    list_for_each(&r->pend_head, p, c) {
                        list_get(&r->pend_head);
                        cdnet_list_put(intf->free_head, c);
                        c = p;
                    }
                }
            }

            cdnet_list_put(intf->free_head, &r->p0_req->node);
            cdnet_list_put(intf->free_head, &r->p0_ans->node);
            r->p0_req = NULL;
            r->p0_ans = NULL;
            r->p0_retry_cnt = 0;
        }
        if (r->p0_ack) {
            list_for_each(&r->pend_head, p, c) {
                cdnet_packet_t *pkt = list_entry(c, cdnet_packet_t);
                if (pkt->_seq_num == r->p0_ack->dat[2])
                    break;
                list_get(&r->pend_head);
                cdnet_list_put(intf->free_head, c);
                c = p;
            }
            cdnet_list_put(intf->free_head, &r->p0_ack->node);
            r->p0_ack = NULL;
        }

        if (r->p0_req) {
            if (get_systick() - r->p0_req->_send_time > SEQ_TIMEOUT) {
                dd_warn(intf->name, "tx: p0_req timeout, len: %d\n",
                        r->p0_req->len);
                if (r->p0_retry_cnt > SEQ_TX_RETRY_MAX) {
                    dd_error(intf->name, "tx: reach retry_max\n");
                    while (r->pend_head.first)
                        cdnet_list_put(intf->free_head, list_get(&r->pend_head));
                    while (r->wait_head.first)
                        cdnet_list_put(intf->free_head, list_get(&r->wait_head));
                    cdnet_list_put(intf->free_head, &r->p0_req->node);
                    r->p0_req = NULL;
                    r->p0_retry_cnt = 0;
                    r->seq_num = 0x80;
                    continue;
                }
                if (cdnet_send_pkt(intf, r->p0_req) == 0) {
                    r->p0_req->_send_time = get_systick();
                    r->p0_retry_cnt++;
                }
            }
            continue;
        }

        if ((r->pend_head.first || r->wait_head.first) && r->seq_num & 0x80) {
            r->p0_req = cdnet_packet_get(intf->free_head);
            if (!r->p0_req) {
                dd_error(intf->name, "tx: set_seq: no free pkt\n");
                continue;
            }
            r->seq_num = 0;
            r->p0_req->level = CDNET_L0; // TODO: add multi_net support
            r->p0_req->dst_mac = r->addr.mac;
            cdnet_fill_src_addr(intf, r->p0_req);
            r->p0_req->src_port = CDNET_DEF_PORT;
            r->p0_req->dst_port = 0;
            r->p0_req->len = 2;
            r->p0_req->dat[0] = 0x00;
            r->p0_req->dat[1] = 0x00;
            if (cdnet_send_pkt(intf, r->p0_req) == 0)
                r->p0_req->_send_time = get_systick();
            else
                r->p0_req->_send_time = get_systick() - SEQ_TIMEOUT;
            dd_debug(intf->name, "tx: set_seq\n");
            continue;
        }

        // check if pend timeout
        if (r->pend_head.first) {
            cdnet_packet_t *pkt = list_entry(r->pend_head.first, cdnet_packet_t);
            if (get_systick() - pkt->_send_time > SEQ_TIMEOUT) {
                dd_warn(intf->name, "tx: pending timeout\n");
                // send check
                r->p0_req = cdnet_packet_get(intf->free_head);
                if (!r->p0_req) {
                    dd_error(intf->name, "tx: chk_seq: no free pkt\n");
                    continue;
                }
                r->p0_req->level = CDNET_L0; // TODO: add multi_net support
                r->p0_req->dst_mac = r->addr.mac;
                cdnet_fill_src_addr(intf, r->p0_req);
                r->p0_req->src_port = CDNET_DEF_PORT;
                r->p0_req->dst_port = 0;
                r->p0_req->len = 0;
                if (cdnet_send_pkt(intf, r->p0_req) == 0)
                    r->p0_req->_send_time = get_systick();
                else
                    r->p0_req->_send_time = get_systick() - SEQ_TIMEOUT;
                r->send_cnt = 0;
            }
        }

        // send wait_head
        list_for_each(&r->wait_head, p, c) {
            int ret;
            cdnet_packet_t *pkt = list_entry(c, cdnet_packet_t);

            if (r->pend_head.len > SEQ_TX_PEND_MAX)
                break;
            if (pkt->seq) {
                pkt->_seq_num = r->seq_num;
                if (++r->send_cnt == SEQ_TX_ACK_CNT) {
                    r->send_cnt = 0;
                    pkt->_req_ack = true;
                } else {
                    pkt->_req_ack = false;
                }
            }
            ret = cdnet_send_pkt(intf, pkt);
            if (ret < 0)
                break;
            list_get(&r->wait_head);
            if (ret == 0 && pkt->seq) {
                r->seq_num = (r->seq_num + 1) & 0x7f;
                pkt->_send_time = get_systick();
                list_put(&r->pend_head, c);
            } else {
                if (ret != 0)
                    dd_error(intf->name, "tx: send wait_head error\n");
                cdnet_list_put(intf->free_head, c);
            }
            c = p;
        }
    }
}
