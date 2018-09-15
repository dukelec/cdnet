/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdbus_uart.h"

#ifdef CDUART_IRQ_SAFE
#define cduart_frame_get(head)  list_get_entry_it(head, cd_frame_t)
#define cduart_list_put         list_put_it
#elif !defined(CDUART_USER_LIST)
#define cduart_frame_get(head)  list_get_entry(head, cd_frame_t)
#define cduart_list_put         list_put
#endif

// member functions

static cd_frame_t *cduart_get_free_frame(cd_intf_t *cd_intf)
{
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    return cduart_frame_get(intf->free_head);
}

static cd_frame_t *cduart_get_rx_frame(cd_intf_t *cd_intf)
{
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    return cduart_frame_get(&intf->rx_head);
}

static void cduart_put_free_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_list_put(intf->free_head, &frame->node);
}

static void cduart_put_tx_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_list_put(&intf->tx_head, &frame->node);
}


void cduart_intf_init(cduart_intf_t *intf, list_head_t *free_head)
{
    if (!intf->name)
        intf->name = "cduart";
    intf->rx_frame = list_get_entry(free_head, cd_frame_t);
    intf->free_head = free_head;
    intf->cd_intf.get_free_frame = cduart_get_free_frame;
    intf->cd_intf.get_rx_frame = cduart_get_rx_frame;
    intf->cd_intf.put_free_frame = cduart_put_free_frame;
    intf->cd_intf.put_tx_frame = cduart_put_tx_frame;

    intf->t_last = get_systick();
    intf->rx_crc = 0xffff;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    intf->rx_byte_cnt = 0;

    // filters should set by caller
    intf->remote_filter_len = 0;
    intf->local_filter_len = 0;
#endif
}

// handler

static bool rx_match_filter(cduart_intf_t *intf,
        cd_frame_t *frame, bool test_remote)
{
    uint8_t i;
    uint8_t *filter;
    uint8_t filter_len;
    uint8_t val;
    bool is_match = false;

    if (test_remote) {
        filter = intf->remote_filter;
        filter_len = intf->remote_filter_len;
        val = frame->dat[0];
    } else {
        filter = intf->local_filter;
        filter_len = intf->local_filter_len;
        val = frame->dat[1];
    }

    for (i = 0; i < filter_len; i++) {
        if (val == *(filter + i)) {
            is_match = true;
            break;
        }
    }
    return is_match;
}

void cduart_rx_handle(cduart_intf_t *intf, const uint8_t *buf, int len)
{
    int i;
    int max_len;
    int cpy_len;
    const uint8_t *rd = buf;

    while (true) {
        cd_frame_t *frame = intf->rx_frame;

        if (!len || rd == buf + len)
            return;
        max_len = buf + len - rd;

        if (intf->rx_byte_cnt != 0 &&
                get_systick() - intf->t_last > CDUART_IDLE_TIME) {
            dn_warn(intf->name, "drop timeout, cnt: %d\n", intf->rx_byte_cnt);
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
        }
        intf->t_last = get_systick();

        if (intf->rx_byte_cnt < 3)
            cpy_len = min(3 - intf->rx_byte_cnt, max_len);
        else
            cpy_len = min(frame->dat[2] + 5 - intf->rx_byte_cnt, max_len);

        memcpy(frame->dat + intf->rx_byte_cnt, rd, cpy_len);
        intf->rx_byte_cnt += cpy_len;

        if (intf->rx_byte_cnt <= 3 &&
                ((intf->rx_byte_cnt >= 2 &&
                        !rx_match_filter(intf, frame, false)) ||
                        (intf->rx_byte_cnt >= 1 &&
                                !rx_match_filter(intf, frame, true)))) {
            dn_warn(intf->name, "filtered, len: %d, [%02x, %02x ...]\n",
                    intf->rx_byte_cnt, frame->dat[0], frame->dat[1]);
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
            return;
        }

        for (i = 0; i < cpy_len; i++)
            crc16_byte(*(rd + i), &intf->rx_crc);
        rd += cpy_len;

        if (intf->rx_byte_cnt == frame->dat[2] + 5) {
            if (intf->rx_crc != 0) {
                dn_error(intf->name, "crc error\n");
            } else {
                cd_frame_t *frm = cduart_frame_get(intf->free_head);
                if (frm) {
#ifdef VERBOSE
                    char pbuf[52];
                    hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
                    dn_verbose(intf->name, "-> [%s]\n", pbuf);
#endif
                    cduart_list_put(&intf->rx_head, &intf->rx_frame->node);
                    intf->rx_frame = frm;
                } else {
                    // set rx_lost flag
                    dn_error(intf->name, "rx_lost\n");
                }
            }
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
        }
    }
}
