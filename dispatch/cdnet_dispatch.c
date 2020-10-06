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

#ifdef CDN_TGT
static void cdn_tgt_routine(cdn_ns_t *ns, cdn_tgt_t *tgt);
static void cdn_tgt_cancel_all(cdn_ns_t *ns, cdn_tgt_t *tgt, int ret_val);
static int cdn_send_p0(cdn_ns_t *ns, cdn_tgt_t *tgt, int type);
#endif


void cdn_routine(cdn_ns_t *ns)
{
    // rx
    for (int i = 0; i < CDN_INTF_MAX; i++) {
        cdn_intf_t *intf = &ns->intfs[i];
        cd_frame_t *frame = NULL;
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
            memset(pkt, 0, offsetof(cdn_pkt_t, dat));
            pkt->_l_net = intf->net;

            if (!(frame->dat[3] & 0x80)) { // rx l0
#ifdef CDN_L0_C
                if (frame->dat[3] & 0x40) { // reply
                    cdn_tgt_t *tgt = cdn_tgt_search(ns, (intf->net << 8) | frame->dat[0], NULL);
                    if (tgt) {
                        pkt->_l0_lp = tgt->_l0_lp;
                        tgt->t_last = get_systick();
                    } else {
                        pkt->_l0_lp = 0;
                    }
                }
#endif
                // addition in: _l_net, _l0_lp (central only)
                ret = cdn0_from_frame(frame->dat, pkt);

                if (ret) {
                    cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                    if (!ret && sock) {
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

                if (ret) {
                    // TODO: check dst net match or not
#ifdef CDN_TGT
                    cdn_tgt_t *tgt = cdn_tgt_search(ns, (pkt->src.addr[1] << 8) | pkt->src.addr[2], NULL);
#endif
                    if (pkt->src.addr[0] & 0x8) { // seq
#ifdef CDN_TGT
                        if (tgt) {
                            tgt->t_last = get_systick();
                            if ((pkt->_seq & 0x7f) == tgt->rx_seq) {
                                tgt->rx_seq = (tgt->rx_seq + 1) & 0x7f;
                                cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                                if (sock) {
                                    cdn_list_put(&sock->rx_head, &pkt->node);
                                } else {
                                    d_verbose("rx: l1 no sock\n");
                                    cdn_list_put(&ns->free_pkts, &pkt->node);
                                }
                            } else {
                                d_verbose("rx: l1 seq: %d != %d\n", pkt->_seq, tgt->rx_seq);
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }
                        } else
#endif
                        { // else
                            d_verbose("rx: l1 no tgt\n");
                            cdn_list_put(&ns->free_pkts, &pkt->node);
                        }
                    } else { // non-seq
#ifdef CDN_TGT
                        if (pkt->src.port == 0 && pkt->dst.port == CDN_DEF_PORT) { // p0 return
                            if (pkt->len == 1 && pkt->dat[0] == 0x80) { // p0 set return
                                if (tgt) {
                                    tgt->t_last = get_systick();
                                    tgt->p0_state |= 0x80;
                                }
                            } else if (pkt->len == 2 && pkt->dat[0] == 0x80) { // p0 check return
                                if (tgt) {
                                    tgt->t_last = get_systick();
                                    tgt->tx_seq_r = pkt->dat[1];
                                    tgt->p0_state |= 0x80;
                                }
                            }
                            cdn_list_put(&ns->free_pkts, &pkt->node);

                        } else if(pkt->src.port == CDN_DEF_PORT && pkt->dst.port == 0) { // p0 report & set
                            if (pkt->len == 1 && pkt->dat[0] == 0x00) { // p0 check
                                pkt->len = 2;
                                pkt->dat[0] = 0x80;
                                pkt->dat[1] = tgt ? tgt->rx_seq : 0xff;
                                swap(pkt->src, pkt->dst);
                                cdn_set_addr(pkt->src.addr, 0x80, intf->net, intf->mac);
                                cdn_send_pkt(ns, pkt);

                            } else if (pkt->len == 2 && pkt->dat[0] == 0x20) { // p0 set
                                if (!tgt) {
                                    tgt = cdn_tgt_get(&ns->free_tgts);
                                    if (tgt) {
                                        memset(tgt, 0, sizeof(cdn_tgt_t));
                                        tgt->id = (pkt->src.addr[1] << 8) | pkt->src.addr[2];
                                        tgt->rx_seq = tgt->tx_seq = 0xff;
                                        tgt->t_tx_last = tgt->t_last = get_systick();
                                        cdn_list_put(&ns->tgts, &tgt->node);
                                    }
                                }
                                if (tgt) {
                                    tgt->t_last = get_systick();
                                    tgt->rx_seq = pkt->dat[1];
                                    pkt->len = 1;
                                    pkt->dat[0] = 0x80;
                                } else {
                                    pkt->len = 1;
                                    pkt->dat[0] = 0x80 | CDN_RET_NO_FREE;
                                }
                                swap(pkt->src, pkt->dst);
                                cdn_set_addr(pkt->src.addr, 0x80, intf->net, intf->mac);
                                cdn_send_pkt(ns, pkt);

                            } else if (pkt->len == 2 && pkt->dat[0] == 0x40) { // p0 report
                                if (tgt) {
                                    tgt->t_last = get_systick();
                                    tgt->tx_seq_r = pkt->dat[1];
                                }
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }

                        } else
#endif // CDN_TGT
                        { // else // rx non-seq pkt
                            cdn_sock_t *sock = cdn_sock_search(ns, pkt->dst.port);
                            if (sock) {
                                cdn_list_put(&sock->rx_head, &pkt->node);
                            } else {
                                d_verbose("rx: l1 no sock\n");
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }
                        }
                    }
                } else {
                    d_verbose("rx: l1 frame err: %d\n", ret);
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                }

            } else { // rx l2
#ifdef CDN_L2
                // addition in: _l_net; out: _seq, _l2_frag, l2_uf
                ret = cdn2_from_frame(frame->dat, pkt);

                if (!ret) {
                    cdn_tgt_t *tgt = cdn_tgt_search(ns, (intf->net << 8) | frame->dat[0], NULL);
                    if (!tgt) {
                        tgt = cdn_tgt_get(&ns->free_tgts);
                        if (tgt) {
                            memset(tgt, 0, sizeof(cdn_tgt_t));
                            tgt->id = (intf->net << 8) | frame->dat[0];
                            tgt->rx_seq = tgt->tx_seq = 0xff;
                            tgt->t_tx_last = tgt->t_last = get_systick();
                            cdn_list_put(&ns->tgts, &tgt->node);
                        }
                    }
                    if (tgt) {
                        tgt->t_last = get_systick();
                        if (pkt->src.addr[0] & 0x8) { // seq
                            if ((pkt->_seq & 0x7f) == tgt->rx_seq) {
                                tgt->rx_seq = (tgt->rx_seq + 1) & 0x7f;
                                if (pkt->_l2_frag <= CDN_FRAG_FIRST && tgt->_l2_rx) {
                                    cdn_list_put(&ns->free_pkts, &tgt->_l2_rx->node);
                                    tgt->_l2_rx = NULL;
                                    d_verbose("rx: l2 drop old _l2_rx\n");
                                }
                                if (pkt->_l2_frag) {
                                    if (tgt->_l2_rx) {
                                        memcpy(tgt->_l2_rx->dat + tgt->_l2_rx->len, pkt->dat, pkt->len);
                                        tgt->_l2_rx->len += pkt->len;
                                        cdn_list_put(&ns->free_pkts, &pkt->node);
                                    } else {
                                        tgt->_l2_rx = pkt;
                                    }
                                    if (pkt->_l2_frag == CDN_FRAG_LAST) {
                                        cdn_list_put(&ns->l2_rx, &tgt->_l2_rx->node);
                                        tgt->_l2_rx = NULL;
                                    }
                                } else { // non-frag
                                    cdn_list_put(&ns->l2_rx, &pkt->node);
                                }
                            } else {
                                d_verbose("rx: l2 seq: %d != %d\n", pkt->_seq, tgt->rx_seq);
                                cdn_list_put(&ns->free_pkts, &pkt->node);
                            }
                        } else { // non-seq
                            cdn_list_put(&ns->l2_rx, &pkt->node);
                        }
                    } else {
                        d_verbose("rx: l2 no free tgt\n");
                        cdn_list_put(&ns->free_pkts, &pkt->node);
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
        }

        if (frame)
            dev->put_free_frame(dev, frame);
    }

    // tgt tx & gc
#ifdef CDN_TGT
    list_node_t *pre, *pos;
    list_for_each(&ns->tgts, pre, pos) {
        cdn_tgt_t *t = container_of(pos, cdn_tgt_t, node);
        cdn_tgt_routine(ns, t);
        if (!t->tgts.len && !t->tx_pend.len && !t->tx_wait.len && get_systick() - t->t_last > CDN_TGT_GC) {
            list_pick(&ns->tgts, pre, pos);
            pos = pre;
#ifdef CDN_L2
            if (t->_l2_rx) {
                cdn_list_put(&ns->free_pkts, &t->_l2_rx->node);
                t->_l2_rx = NULL;
            }
#endif
            cdn_list_put(&ns->free_tgts, &t->node);
        }
    }
#endif // CDN_TGT
}


#ifdef CDN_TGT

static void cdn_tgt_routine(cdn_ns_t *ns, cdn_tgt_t *tgt)
{
    list_node_t *pre, *pos;

    if (tgt->tgts.len) { // mcast tgt
        cdn_tgt_t *st = container_of(tgt->tgts.first, cdn_tgt_t, node);
        tgt->tx_seq_r = st->tx_seq_r;
        bool all_finish = true;
        list_for_each(&tgt->tgts, pre, pos) {
            st = container_of(pos, cdn_tgt_t, node);
            if (!(tgt->tx_seq_r & 0x80)) { // find the oldest tx_seq_r
                if ((st->tx_seq_r & 0x80)
                        || (st->tx_seq_r < tgt->tx_seq_r && tgt->tx_seq_r - st->tx_seq_r < 64)
                        || (st->tx_seq_r > tgt->tx_seq_r && st->tx_seq_r - tgt->tx_seq_r > 64))
                    tgt->tx_seq_r = st->tx_seq_r;
            }
            if (!(st->p0_state & 0x80))
                all_finish = false;
        }
        if (all_finish) {
            tgt->p0_state |= 0x80;
            list_for_each(&tgt->tgts, pre, pos) {
                st = container_of(pos, cdn_tgt_t, node);
                st->p0_state = 0;
            }
        }
    }

    if (tgt->p0_state) {
        tgt->t_last = get_systick();

        if (tgt->p0_state & 0x80) { // finish

            if (tgt->p0_state == 0x82) {
                tgt->tx_seq = 0; // init 0
                cdn_tgt_cancel_all(ns, tgt, CDN_RET_SEQ_ERR);
            }

            if (tgt->tx_seq_r & 0x80) { // cancel all - seq_err
                tgt->tx_seq = 0xff;
                cdn_tgt_cancel_all(ns, tgt, CDN_RET_SEQ_ERR);

            } else { // free succeeded
                bool resend = false;
                list_for_each(&tgt->tx_pend, pre, pos) {
                    cdn_pkt_t *p = container_of(pos, cdn_pkt_t, node);
                    if (tgt->tx_seq_r == p->_seq)
                        resend = true;
                    if (!resend) {
                        list_pick(&tgt->tx_pend, pre, pos);
                        pos = pre;
                        p->ret = 0x80; // succeeded
                        if (!(p->conf & CDN_CONF_NOT_FREE))
                            cdn_list_put(&ns->free_pkts, &p->node);
                    } else { // resend
                        if (!(p->_seq & 0x80))
                            cdn_send_frame(ns, p);
                    }
                }
            }

            tgt->p0_state = 0;
            tgt->p0_retry = 0;

        } else {
            if (get_systick() - tgt->t_tx_last >= CDN_SEQ_TIMEOUT) {
                if (++tgt->p0_retry < CDN_SEQ_P0_RETRY_MAX) {
                    cdn_send_p0(ns, tgt, tgt->p0_state);

                } else { // cancel all - timeout
                    tgt->p0_state = 0;
                    tgt->p0_retry = 0;
                    tgt->tx_seq = 0xff;
                    cdn_tgt_cancel_all(ns, tgt, CDN_RET_TIMEOUT);
                }
            }
            return;
        }

    } else if (!(tgt->tx_seq_r & 0x80)) {
        list_for_each(&tgt->tx_pend, pre, pos) {
            cdn_pkt_t *p = container_of(pos, cdn_pkt_t, node);
            if (tgt->tx_seq_r == p->_seq)
                break;
            list_pick(&tgt->tx_pend, pre, pos);
            pos = pre;
            p->ret = 0x80; // succeeded
            if (!(p->conf & CDN_CONF_NOT_FREE))
                cdn_list_put(&ns->free_pkts, &p->node);
            tgt->t_last = get_systick();
        }
    }

    if (tgt->tx_pend.len < CDN_SEQ_TX_PEND_MAX && tgt->tx_wait.len) {
        if (tgt->tx_seq & 0x80) {
            cdn_send_p0(ns, tgt, 2); // set 0
            return;

        } else {
            while (tgt->tx_pend.len < CDN_SEQ_TX_PEND_MAX) {
                cdn_pkt_t *p = cdn_pkt_get(&tgt->tx_wait);
                if (!p)
                    break;
                if (p->src.addr[0] == 0xc8 && p->len > CDN2_MTU - 2) { // original big l2 pkt
                    p->_seq = 0xff;
                    cdn_list_put(&tgt->tx_pend, &p->node);
                } else {
                    if (++tgt->tx_cnt > CDN_SEQ_ACK_CNT || (p->conf & CDN_CONF_REQ_ACK)) {
                        tgt->tx_cnt = 0;
                        p->_seq = tgt->tx_seq | 0x80;
                    } else {
                        p->_seq = tgt->tx_seq;
                    }
                    int ret = cdn_send_frame(ns, p);
                    if (!ret) {
                        tgt->tx_seq = (tgt->tx_seq + 1) & 0x7f;
                        p->_seq &= 0x7f;
                        cdn_list_put(&tgt->tx_pend, &p->node);
                    } else {
                        p->ret = 0x80 | ret;
                        if (!(p->conf & CDN_CONF_NOT_FREE))
                            cdn_list_put(&ns->free_pkts, &p->node);
                    }
                }
                tgt->t_tx_last = tgt->t_last = get_systick();
            }
        }
    }

    if (tgt->tx_pend.len && get_systick() - tgt->t_tx_last > CDN_SEQ_TIMEOUT)
        cdn_send_p0(ns, tgt, 1); // get
}


static void cdn_tgt_cancel_all(cdn_ns_t *ns, cdn_tgt_t *tgt, int ret_val)
{
    list_node_t *pre, *pos;

    list_for_each(&tgt->tx_pend, pre, pos) {
        cdn_pkt_t *p = container_of(pos, cdn_pkt_t, node);
        list_pick(&tgt->tx_pend, pre, pos);
        pos = pre;
        p->ret = 0x80 | ret_val;
        if (!(p->conf & CDN_CONF_NOT_FREE))
            cdn_list_put(&ns->free_pkts, &p->node);
    }
    list_for_each(&tgt->tx_wait, pre, pos) {
        cdn_pkt_t *p = container_of(pos, cdn_pkt_t, node);
        list_pick(&tgt->tx_pend, pre, pos);
        pos = pre;
        p->ret = 0x80 | ret_val;
        if (!(p->conf & CDN_CONF_NOT_FREE))
            cdn_list_put(&ns->free_pkts, &p->node);
    }
}

static int cdn_send_p0(cdn_ns_t *ns, cdn_tgt_t *tgt, int type)
{
    list_node_t *pos;
    cdn_intf_t *intf;
    int idx;

    if (tgt->tgts.len) { // TODO: support multi-intf for mcast
        cdn_tgt_t *st = container_of(tgt->tgts.first, cdn_tgt_t, node);
        intf = cdn_intf_search(ns, st->id >> 8, true, &idx);
    } else {
        intf = cdn_intf_search(ns, tgt->id >> 8, true, &idx);
    }
    if (!intf)
        return -1;

    cdn_pkt_t *p = cdn_pkt_get(&ns->free_pkts);
    if (!p)
        return -1;
    memset(p, 0, offsetof(cdn_pkt_t, dat));
    p->src.port = CDN_DEF_PORT;
    p->dst.port = 0;
    cdn_set_addr(p->dst.addr, tgt->tgts.len ? 0xf0 : (idx >= 0 ? 0xa0 : 0x80), tgt->id >> 8, tgt->id & 0xff);
    if (type == 1) { // get
        p->len = 1;
        p->dat[0] = 0x00;
        tgt->p0_state = 1;
    } else { // reset to 0
        p->len = 2;
        p->dat[0] = 0x20;
        p->dat[1] = 0x00;
        tgt->p0_state = 2;
    }
    tgt->p0_state = type;
    list_for_each_ro(&tgt->tgts, pos) {
        cdn_tgt_t *st = container_of(pos, cdn_tgt_t, node);
        st->p0_state = type;
    }
    cdn_send_pkt(ns, p);
    tgt->t_tx_last = tgt->t_last = get_systick();
    return 0;
}

#endif // CDN_TGT


int cdn_send_frame(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    int ret = CDN_RET_FMT_ERR;
    cdn_intf_t *intf = cdn_route(ns, pkt);
    if (!intf) {
        d_verbose("tx: no intf found\n");
        return CDN_RET_ROUTE_ERR;
    }

    cd_frame_t *frame = intf->dev->get_free_frame(intf->dev);
    if (!frame)
        return CDN_RET_NO_FREE;

    if (pkt->src.addr[0] == 0) {
        ret = cdn0_to_frame(pkt, frame->dat);
    } else if (!(pkt->src.addr[0] & 0x40)) {
        ret = cdn1_to_frame(pkt, frame->dat);
    } else {
#ifdef CDN_L2
        ret = cdn2_to_frame(pkt, frame->dat);
#endif
    }

    if (ret) {
        intf->dev->put_free_frame(intf->dev, frame);
        return CDN_RET_FMT_ERR;
    } else {
        intf->dev->put_tx_frame(intf->dev, frame);
        return 0;
    }
}


int cdn_send_pkt(cdn_ns_t *ns, cdn_pkt_t *pkt)
{
    int ret;

    if (pkt->dst.addr[0] == 0) { // l0
#ifdef CDN_L0_C
        if (pkt->src.port == CDN_DEF_PORT) {
            cdn_tgt_t *tgt = cdn_tgt_search(ns, (pkt->dst.addr[1] << 8) | pkt->dst.addr[2], NULL);
            if (!tgt) {
                tgt = cdn_tgt_get(&ns->free_tgts);
                if (tgt) {
                    memset(tgt, 0, sizeof(cdn_tgt_t));
                    tgt->id = (pkt->dst.addr[1] << 8) | pkt->dst.addr[2];
                    tgt->rx_seq = tgt->tx_seq = 0xff;
                    tgt->t_tx_last = tgt->t_last = get_systick();
                    cdn_list_put(&ns->tgts, &tgt->node);
                } else {
                    pkt->ret = 0x80 | CDN_RET_NO_FREE;
                    if (!(pkt->conf & CDN_CONF_NOT_FREE))
                        cdn_list_put(&ns->free_pkts, &pkt->node);
                    return CDN_RET_NO_FREE;
                }
            }
            tgt->_l0_lp = pkt->dst.port;
        }
#endif
        ret = cdn_send_frame(ns, pkt);
        pkt->ret = 0x80 | ret;
        if (!(pkt->conf & CDN_CONF_NOT_FREE))
            cdn_list_put(&ns->free_pkts, &pkt->node);
        return ret;

    } else if (!(pkt->dst.addr[0] & 0x40) || (pkt->dst.addr[0] & 0xf0) == 0xf0) { // l1

        if (pkt->dst.addr[0] & 0x8) { // seq
#ifdef CDN_TGT
            cdn_tgt_t *tgt = cdn_tgt_search(ns, (pkt->dst.addr[1] << 8) | pkt->dst.addr[2], NULL);
            if (!tgt) {
                if ((pkt->dst.addr[0] & 0xf0) != 0xf0) // not alloc for mcast
                    tgt = cdn_tgt_get(&ns->free_tgts);
                if (tgt) {
                    memset(tgt, 0, sizeof(cdn_tgt_t));
                    tgt->id = (pkt->dst.addr[1] << 8) | pkt->dst.addr[2];
                    tgt->rx_seq = tgt->tx_seq = 0xff;
                    tgt->t_tx_last = tgt->t_last = get_systick();
                    cdn_list_put(&ns->tgts, &tgt->node);
                } else {
                    pkt->ret = 0x80 | CDN_RET_NO_FREE;
                    if (!(pkt->conf & CDN_CONF_NOT_FREE))
                        cdn_list_put(&ns->free_pkts, &pkt->node);
                    return CDN_RET_NO_FREE;
                }
            }
            cdn_list_put(&tgt->tx_wait, &pkt->node);
            return 0;
#else
            pkt->ret = 0x80 | CDN_RET_FMT_ERR;
            if (!(pkt->conf & CDN_CONF_NOT_FREE))
                cdn_list_put(&ns->free_pkts, &pkt->node);
            return CDN_RET_FMT_ERR;
#endif // CDN_TGT
        } else {
            ret = cdn_send_frame(ns, pkt);
            pkt->ret = 0x80 | ret;
            if (!(pkt->conf & CDN_CONF_NOT_FREE))
                cdn_list_put(&ns->free_pkts, &pkt->node);
            return ret;
        }

    } else { // l2
#ifdef CDN_L2
        if (pkt->dst.addr[0] & 0x8) { // seq

            cdn_tgt_t *tgt = cdn_tgt_search(ns, (pkt->dst.addr[1] << 8) | pkt->dst.addr[2], NULL);
            if (!tgt) {
                tgt = cdn_tgt_get(&ns->free_tgts);
                if (tgt) {
                    memset(tgt, 0, sizeof(cdn_tgt_t));
                    tgt->id = (pkt->dst.addr[1] << 8) | pkt->dst.addr[2];
                    tgt->rx_seq = tgt->tx_seq = 0xff;
                    tgt->t_tx_last = tgt->t_last = get_systick();
                    cdn_list_put(&ns->tgts, &tgt->node);
                } else {
                    pkt->ret = 0x80 | CDN_RET_NO_FREE;
                    if (!(pkt->conf & CDN_CONF_NOT_FREE))
                        cdn_list_put(&ns->free_pkts, &pkt->node);
                    return CDN_RET_NO_FREE;
                }
            }

            int n = (pkt->len + (CDN2_MTU - 2 - 1)) / (CDN2_MTU - 2);

            if (n == 1 && ns->free_pkts.len) {
                pkt->_l2_frag = CDN_FRAG_NONE;
                cdn_list_put(&tgt->tx_wait, &pkt->node);
                return 0;

            } else if (n <= ns->free_pkts.len) {
                for (int i = 0; i < n; i++) {
                    cdn_pkt_t *p = cdn_pkt_get(&ns->free_pkts);
                    uint32_t offset = i * (CDN2_MTU - 2);
                    uint32_t len = min(pkt->len - offset, CDN2_MTU - 2);
                    memcpy(p, pkt, offsetof(cdn_pkt_t, dat));
                    memcpy(p->dat, pkt->dat + offset, len);
                    p->_l2_frag = i == 0 ? CDN_FRAG_FIRST : (i + 1 == n ? CDN_FRAG_LAST : CDN_FRAG_MORE);
                    cdn_list_put(&tgt->tx_wait, &p->node);
                }

                if (!(pkt->conf & CDN_CONF_NOT_FREE))
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                else
                    cdn_list_put(&tgt->tx_wait, &pkt->node); // release after success
                return 0;

            } else {
                d_verbose("tx: no pkt for l2 split\n");
                pkt->ret = 0x80 | CDN_RET_NO_FREE;
                if (!(pkt->conf & CDN_CONF_NOT_FREE))
                    cdn_list_put(&ns->free_pkts, &pkt->node);
                return CDN_RET_NO_FREE;
            }

        } else {
            pkt->_l2_frag = 0;
            ret = cdn_send_frame(ns, pkt);
            pkt->ret = 0x80 | ret;
            if (!(pkt->conf & CDN_CONF_NOT_FREE))
                cdn_list_put(&ns->free_pkts, &pkt->node);
            return ret;
        }
#else
        pkt->ret = 0x80 | CDN_RET_FMT_ERR;
        if (!(pkt->conf & CDN_CONF_NOT_FREE))
            cdn_list_put(&ns->free_pkts, &pkt->node);
        return CDN_RET_FMT_ERR;
#endif // CDN_L2
    }

    return 0;
}


int cdn_sock_bind(cdn_sock_t *sock)
{
    return cdn_sock_insert(sock);
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
