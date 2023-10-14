/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdbus_uart.h"
#include "cd_debug.h"

#if CD_FRAME_SIZE < 258
#error "CD_FRAME_SIZE must be at least 258 bytes to store the CRC!"
#elif CD_FRAME_SIZE > 260
#error "CD_FRAME_SIZE is too large!"
#endif

#ifdef CDUART_IRQ_SAFE
#define cduart_frame_get(head)  list_get_entry_it(head, cd_frame_t)
#define cduart_list_put         list_put_it
#elif !defined(CDUART_USER_LIST)
#define cduart_frame_get(head)  list_get_entry(head, cd_frame_t)
#define cduart_list_put         list_put
#endif

// member functions

static cd_frame_t *cduart_get_free_frame(cd_dev_t *cd_dev)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    return cduart_frame_get(dev->free_head);
}

static cd_frame_t *cduart_get_rx_frame(cd_dev_t *cd_dev)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    return cduart_frame_get(&dev->rx_head);
}

static void cduart_put_free_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    cduart_list_put(dev->free_head, &frame->node);
}

static void cduart_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    cduart_list_put(&dev->tx_head, &frame->node);
}


void cduart_dev_init(cduart_dev_t *dev, list_head_t *free_head)
{
    if (!dev->name)
        dev->name = "cduart";
    dev->rx_frame = list_get_entry(free_head, cd_frame_t);
    dev->free_head = free_head;
    dev->cd_dev.get_free_frame = cduart_get_free_frame;
    dev->cd_dev.get_rx_frame = cduart_get_rx_frame;
    dev->cd_dev.put_free_frame = cduart_put_free_frame;
    dev->cd_dev.put_tx_frame = cduart_put_tx_frame;

    dev->t_last = get_systick();
    dev->rx_crc = 0xffff;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->rx_byte_cnt = 0;

    // filters should set by caller
    dev->remote_filter_len = 0;
    dev->local_filter_len = 0;
#endif
}

// handler

static bool rx_match_filter(cduart_dev_t *dev,
        cd_frame_t *frame, bool test_remote)
{
    uint8_t i;
    uint8_t *filter;
    uint8_t filter_len;
    uint8_t val;
    bool is_match = false;

    if (test_remote) {
        filter = dev->remote_filter;
        filter_len = dev->remote_filter_len;
        val = frame->dat[0];
    } else {
        filter = dev->local_filter;
        filter_len = dev->local_filter_len;
        val = frame->dat[1];
    }

    if (!filter_len)
        return true;

    for (i = 0; i < filter_len; i++) {
        if (val == *(filter + i)) {
            is_match = true;
            break;
        }
    }
    return is_match;
}

void cduart_rx_handle(cduart_dev_t *dev, const uint8_t *buf, int len)
{
    int i;
    int max_len;
    int cpy_len;
    int frame_len_safe;
    const uint8_t *rd = buf;

    while (true) {
        cd_frame_t *frame = dev->rx_frame;

        if (!len || rd == buf + len)
            return;
        max_len = buf + len - rd;

        if (dev->rx_byte_cnt != 0 &&
                get_systick() - dev->t_last > CDUART_IDLE_TIME) {
            dn_warn(dev->name, "drop timeout, cnt: %d\n", dev->rx_byte_cnt);
            dev->rx_byte_cnt = 0;
            dev->rx_crc = 0xffff;
        }
        dev->t_last = get_systick();

        if (dev->rx_byte_cnt < 3) {
            cpy_len = min(3 - dev->rx_byte_cnt, max_len);
        } else {
            frame_len_safe = min(frame->dat[2], CD_FRAME_SIZE - 5);
            cpy_len = min(frame_len_safe + 5 - dev->rx_byte_cnt, max_len);
        }

        memcpy(frame->dat + dev->rx_byte_cnt, rd, cpy_len);
        dev->rx_byte_cnt += cpy_len;

        if (dev->rx_byte_cnt <= 3 &&
                ((dev->rx_byte_cnt >= 2 &&
                        !rx_match_filter(dev, frame, false)) ||
                        (dev->rx_byte_cnt >= 1 &&
                                !rx_match_filter(dev, frame, true)))) {
            dn_warn(dev->name, "filtered, len: %d, [%02x, %02x ...]\n",
                    dev->rx_byte_cnt, frame->dat[0], frame->dat[1]);
            dev->rx_byte_cnt = 0;
            dev->rx_crc = 0xffff;
            return;
        }

        for (i = 0; i < cpy_len; i++)
            crc16_byte(*(rd + i), &dev->rx_crc);
        rd += cpy_len;

        frame_len_safe = min(frame->dat[2], CD_FRAME_SIZE - 5);
        if (dev->rx_byte_cnt == frame_len_safe + 5) {
            if (dev->rx_crc != 0) {
                dn_error(dev->name, "crc error\n");
                dev->rx_byte_cnt = 0;
                dev->rx_crc = 0xffff;
                return;
            } else {
                cd_frame_t *frm = cduart_frame_get(dev->free_head);
                if (frm) {
#ifdef VERBOSE
                    char pbuf[52];
                    hex_dump_small(pbuf, frame->dat, frame_len_safe + 3, 16);
                    dn_verbose(dev->name, "-> [%s]\n", pbuf);
#endif
                    cduart_list_put(&dev->rx_head, &dev->rx_frame->node);
                    dev->rx_frame = frm;
                } else {
                    // set rx_lost flag
                    dn_error(dev->name, "rx_lost\n");
                }
                dev->rx_byte_cnt = 0;
                dev->rx_crc = 0xffff;
            }
        }
    }
}
