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


void cdnet_intf_init(cdnet_intf_t *intf, list_head_t *free_head,
        cd_intf_t *cd_intf, uint8_t mac)
{
    intf->mac = mac;    // set to 255 if auto alloc IP addr
    intf->free_head = free_head;

    intf->cd_intf = cd_intf;

#ifdef USE_DYNAMIC_INIT
    intf->net_id = 0;
    intf->last_basic_port = 0;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    list_head_init(&intf->rx_frag_head);
    list_head_init(&intf->tx_frag_head);
#endif
}

static int cdnet_to_frame(cdnet_intf_t *intf,
        cdnet_packet_t *pkt, uint8_t *buf)
{
    int i;
    int ret_val = 0;
    uint8_t *buf_s = buf;
    uint16_t left_data_len;
    uint16_t cal_frame_len;
    uint16_t cal_dat_len;
    bool skip_appends = false;

    // CDBUS frame header: [src, dst, len]
    *buf++ = pkt->src_mac;
    *buf++ = pkt->dst_mac;
    buf++; // fill at end

    // basic format
    if (pkt->src_port == CDNET_BASIC_PORT ||
            pkt->dst_port == CDNET_BASIC_PORT) {
        cal_dat_len = pkt->dat_len;
        pkt->frag_at = pkt->dat;

        if (pkt->src_port == CDNET_BASIC_PORT) { // out request
            assert(pkt->dst_port <= 63);
            intf->last_basic_port = pkt->dst_port;
            *buf++ = pkt->dst_port;
        } else { // out reply
            if (pkt->dat_len >= 1 && pkt->dat[0] <= 31) {
                // share first byte
                pkt->dat[0] |= HDR_BASIC_REPLY | HDR_BASIC_SHARE;
            } else {
                *buf++ = HDR_BASIC_REPLY;
            }
        }

    // standard format
    } else {
        *buf++ |= HDR_STANDARD | (pkt->is_compressed ? HDR_COMPRESSED : 0);
        if (!pkt->is_local) {
            buf[3] |= HDR_FULL_ADDR | (pkt->is_multicast ? HDR_MULTICAST : 0);
            *buf++ = pkt->src_addr[0];
            *buf++ = pkt->src_addr[1];
            *buf++ = pkt->dst_addr[0];
            *buf++ = pkt->dst_addr[1];
        } else if (pkt->is_multicast) {
            buf[3] |= HDR_MULTICAST;
            *buf++ = pkt->dst_addr[1];
        }

        // fragmentation

        if (!pkt->in_fragment)
            left_data_len = pkt->dat_len;
        else
            left_data_len = pkt->dat_len - (pkt->frag_at - pkt->dat);

        cal_frame_len = buf - buf_s + left_data_len;
        cal_frame_len += (pkt->pkt_type == PKT_TYPE_UDP) ? 0 : 1;
        cal_frame_len += (pkt->src_port & 0xff00) ? 2 : 1;
        cal_frame_len += (pkt->dst_port & 0xff00) ? 2 : 1;

        if (!pkt->in_fragment) {
            pkt->frag_at = pkt->dat;

            if (cal_frame_len > 256) {
                ret_val = RET_NOT_FINISH;
                pkt->in_fragment = true;
                pkt->frag_cnt = 0;
                cal_dat_len = left_data_len - (cal_frame_len - 256);
                buf[3] |= HDR_FRAGMENT;
                *buf++ = pkt->frag_cnt++;
            } else {
                cal_dat_len = left_data_len;
            }
        } else {
            buf[3] |= HDR_FRAGMENT;
            skip_appends = true;

            if (cal_frame_len > 256) {
                ret_val = RET_NOT_FINISH;
                *buf++ = pkt->frag_cnt++;
                assert((pkt->frag_cnt & HDR_FRAGMENT_END) == 0);
                cal_dat_len = left_data_len - (cal_frame_len - 256);
            } else {
                assert((pkt->frag_cnt & HDR_FRAGMENT_END) == 0);
                pkt->frag_cnt |= HDR_FRAGMENT_END;
                *buf++ = pkt->frag_cnt;
                pkt->in_fragment = false;
                cal_dat_len = left_data_len;
            }
        }

        if (pkt->pkt_type != PKT_TYPE_UDP) {
            assert(pkt->pkt_type <= PKT_TYPE_END);
            buf[3] |= HDR_FURTHER_PROT;
            if (!skip_appends)
                *buf++ = pkt->pkt_type;
        }

        if (!skip_appends) {
            *buf++ = pkt->src_port & 0xff; // type if icmp
            if (pkt->src_port & 0xff00) {
                *buf++ = pkt->src_port >> 8;
                buf[3] |= HDR_SRC_PORT_16;
            }
            *buf++ = pkt->dst_port & 0xff; // code if icmp
            if (pkt->dst_port & 0xff00) {
                *buf++ = pkt->dst_port >> 8;
                buf[3] |= HDR_DST_PORT_16;
            }
        }
    }

    assert(buf - buf_s + cal_dat_len <= 256);

    for (i = 0; i < cal_dat_len; i++)
        *buf++ = *pkt->frag_at++;
    *(buf_s + 2) = buf - buf_s - 3;

    return ret_val;
}

static int cdnet_from_frame(cdnet_intf_t *intf,
        const uint8_t *buf, cdnet_packet_t *pkt)
{
    int i;
    int ret_val = 0;
    const uint8_t *buf_s = buf;
    uint8_t payload_len;

    if (!pkt->in_fragment) {
        pkt->src_mac = *buf++;
        pkt->dst_mac = *buf++;
        payload_len = *buf++;
        pkt->frag_at = pkt->dat;
        pkt->dat_len = 0;

        if (!(buf[3] & HDR_STANDARD)) { // basic format
            pkt->is_local = true;
            pkt->is_multicast = false;
            pkt->is_compressed = false;

            if (buf[3] & HDR_BASIC_REPLY) { // in reply
                pkt->src_port = intf->last_basic_port;
                pkt->dst_port = CDNET_BASIC_PORT;
                if (buf[3] & HDR_BASIC_SHARE) {
                    *pkt->frag_at++ = *buf++ & 0x1f;
                    pkt->dat_len++;
                }
            } else { // in request
                pkt->src_port = CDNET_BASIC_PORT;
                pkt->dst_port = *buf++;
            }
        } else { // standard format
            pkt->is_local = true;
            pkt->is_multicast = false;
            pkt->is_compressed = !!(buf[3] & HDR_COMPRESSED);

            if (buf[3] & HDR_FULL_ADDR) {
                pkt->is_local = false;
                pkt->is_multicast = !!(buf[3] & HDR_MULTICAST);
                pkt->src_addr[0] = *buf++;
                pkt->src_addr[1] = *buf++;
                pkt->dst_addr[0] = *buf++;
                pkt->dst_addr[1] = *buf++;
            } else if (buf[3] & HDR_MULTICAST) {
                pkt->is_multicast = true;
                pkt->dst_addr[1] = *buf++;
            }

            pkt->frag_cnt = 0;
            if (buf[3] & HDR_FRAGMENT) {
                if (*buf++ != 0)
                    return ERR_PKT_ORDER;
                ret_val = RET_NOT_FINISH;
                pkt->in_fragment = true;
            }

            if (buf[3] & HDR_FURTHER_PROT) {
                pkt->pkt_type = *buf++;
                assert(pkt->pkt_type <= PKT_TYPE_END);
            }

            pkt->src_port = *buf++;
            if (buf[3] & HDR_SRC_PORT_16)
                pkt->src_port |= *buf++ << 8;
            pkt->dst_port = *buf++;
            if (buf[3] & HDR_DST_PORT_16)
                pkt->dst_port |= *buf++ << 8;
        }

    } else { // segment
        if (pkt->src_mac != *buf++)
            return RET_PKT_NOT_MATCH;
        if (pkt->dst_mac != *buf++)
            return RET_PKT_NOT_MATCH;
        payload_len = *buf++;
        assert((buf[3] & HDR_STANDARD) != 0);

        if (pkt->is_multicast != !!(buf[3] & HDR_MULTICAST))
            return RET_PKT_NOT_MATCH;

        if (buf[3] & HDR_FULL_ADDR) {
            if (pkt->is_local != false)
                return RET_PKT_NOT_MATCH;
            if (pkt->src_addr[0] != *buf++)
                return RET_PKT_NOT_MATCH;
            if (pkt->src_addr[1] != *buf++)
                return RET_PKT_NOT_MATCH;
            if (pkt->dst_addr[0] != *buf++)
                return RET_PKT_NOT_MATCH;
            if (pkt->dst_addr[1] != *buf++)
                return RET_PKT_NOT_MATCH;
        } else {
            if (pkt->is_local != true)
                return RET_PKT_NOT_MATCH;
            if (buf[3] & HDR_MULTICAST) {
                if (pkt->dst_addr[1] != *buf++)
                    return RET_PKT_NOT_MATCH;
            }
        }

        assert((buf[3] & HDR_FRAGMENT) != 0);
        if (*buf & HDR_FRAGMENT_END)
            pkt->in_fragment = false;
        else
            ret_val = RET_NOT_FINISH;

        if (pkt->frag_cnt++ != (*buf++ & ~HDR_FRAGMENT_END))
            return ERR_PKT_ORDER;
    }

    payload_len -= (buf - buf_s - 3);
    pkt->dat_len += payload_len;
    for (i = 0; i < payload_len; i++)
        *pkt->frag_at++ = *buf++;

    return ret_val;
}


// helper

void cdnet_exchange_src_dst(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    uint8_t tmp_addr[2];
    uint8_t tmp_mac;

    tmp_mac = pkt->src_mac;
    pkt->src_mac = pkt->dst_mac;
    pkt->dst_mac = tmp_mac;

    if (pkt->is_multicast)
        pkt->src_mac = intf->mac;

    if (!pkt->is_local) {
        memcpy(tmp_addr, pkt->src_addr, 2);
        memcpy(pkt->src_addr, pkt->dst_addr, 2);
        memcpy(pkt->dst_addr, tmp_addr, 2);
        if (pkt->is_multicast) {
            pkt->src_addr[0] = intf->net_id;
            pkt->src_addr[1] = intf->mac;
        }
    }

    if (pkt->pkt_type != PKT_TYPE_ICMP) {
        uint16_t tmp_port = pkt->src_port;
        pkt->src_port = pkt->dst_port;
        pkt->dst_port = tmp_port;
    }
}

void cdnet_fill_src_addr(cdnet_intf_t *intf, cdnet_packet_t *pkt)
{
    pkt->src_mac = intf->mac;

    if (!pkt->is_local) {
        pkt->src_addr[0] = intf->net_id;
        pkt->src_addr[1] = intf->mac;
    }
}

//

void cdnet_rx(cdnet_intf_t *intf)
{
    list_node_t *cd_node, *net_node;
    cd_frame_t *cd_frame;
    cdnet_packet_t *net_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    // TODO: add address filter
    while (true) {
        if (!intf->free_head->first)
            break;

        cd_node = cd_intf->get_rx_node(cd_intf);
        if (!cd_node)
            break;
        cd_frame = container_of(cd_node, cd_frame_t, node);

        while (true) {
            if (intf->rx_frag_head.first) {
                net_node = list_get(&intf->rx_frag_head);
                net_pkt = container_of(net_node, cdnet_packet_t, node);
            } else {
                net_node = list_get(intf->free_head);
                net_pkt = container_of(net_node, cdnet_packet_t, node);
                net_pkt->in_fragment = false;
            }

            ret_val = cdnet_from_frame(intf, cd_frame->dat, net_pkt);

            // TODO: add timeout counter for pkt
            if (ret_val == RET_PKT_NOT_MATCH)
                continue;

            cd_intf->put_free_node(cd_intf, cd_node);

            if (ret_val == 0) {
                list_put(&intf->rx_head, net_node);
            } else if (ret_val == RET_NOT_FINISH) {
                list_put(&intf->rx_frag_head, net_node);
            } else {
                list_put(intf->free_head, net_node);
                d_error("cdnet %p: cdnet_from_frame failed\n", intf);
            }
            break;
        }
    }
}

void cdnet_tx(cdnet_intf_t *intf)
{
    list_node_t *cd_node, *net_node;
    cd_frame_t *cd_frame;
    cdnet_packet_t *net_pkt;
    cd_intf_t *cd_intf = intf->cd_intf;
    int ret_val;

    while (true) {
        net_node = list_get(&intf->rx_head);
        if (!net_node)
            break;
        list_put(intf->free_head, net_node);
    }

    while (true) {
        if (!intf->tx_head.first && !intf->tx_frag_head.first)
            break;

        cd_node = cd_intf->get_free_node(cd_intf);
        if (!cd_node)
            break;

        cd_frame = container_of(cd_node, cd_frame_t, node);
        if (intf->tx_frag_head.first) {
            net_node = list_get(&intf->tx_frag_head);
            net_pkt = container_of(net_node, cdnet_packet_t, node);
        } else {
            net_node = list_get(&intf->tx_head);
            net_pkt = container_of(net_node, cdnet_packet_t, node);
            net_pkt->in_fragment = false;
        }

        ret_val = cdnet_to_frame(intf, net_pkt, cd_frame->dat);

        if (ret_val == 0 || ret_val == RET_NOT_FINISH) {
            cd_intf->put_tx_node(cd_intf, cd_node);
        } else {
            cd_intf->put_free_node(cd_intf, cd_node);
            d_error("cdnet %p: cdnet_to_frame failed\n", intf);
        }

        if (ret_val == RET_NOT_FINISH)
            list_put(&intf->tx_frag_head, net_node);
        else
            list_put(intf->free_head, net_node);
    }
}

