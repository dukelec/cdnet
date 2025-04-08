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


static cd_frame_t *cduart_get_rx_frame(cd_dev_t *cd_dev)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    return cd_list_get(&dev->rx_head);
}

static void cduart_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cduart_dev_t *dev = container_of(cd_dev, cduart_dev_t, cd_dev);
    cd_list_put(&dev->tx_head, frame);
}


void cduart_dev_init(cduart_dev_t *dev, list_head_t *free_head)
{
    if (!dev->name)
        dev->name = "cduart";
    dev->rx_frame = cd_list_get(free_head);
    dev->free_head = free_head;
    dev->cd_dev.get_rx_frame = cduart_get_rx_frame;
    dev->cd_dev.put_tx_frame = cduart_put_tx_frame;

    dev->t_last = get_systick();
    dev->rx_crc = 0xffff;
    dev->local_mac = 0xff; // local_mac should update by caller

#ifdef CD_USE_DYNAMIC_INIT
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->rx_byte_cnt = 0;
    dev->rx_drop = false;
#endif
}


void cduart_rx_handle(cduart_dev_t *dev, const uint8_t *buf, unsigned len)
{
    unsigned max_len;
    unsigned cpy_len;
    const uint8_t *rd = buf;

    while (true) {
        cd_frame_t *frame = dev->rx_frame;

        if (dev->rx_byte_cnt != 0 && get_systick() - dev->t_last > CDUART_IDLE_TIME) {
            dn_warn(dev->name, "drop timeout, cnt: %d, hdr: %02x %02x %02x\n",
                    dev->rx_byte_cnt, frame->dat[0], frame->dat[1], frame->dat[2]);
            dev->rx_byte_cnt = 0;
            dev->rx_crc = 0xffff;
            dev->rx_drop = false;
        }

        if (!len || rd == buf + len)
            return;
        max_len = buf + len - rd;
        dev->t_last = get_systick();

        if (dev->rx_byte_cnt < 3)
            cpy_len = min(3 - dev->rx_byte_cnt, max_len);
        else
            cpy_len = min(frame->dat[2] + 5 - dev->rx_byte_cnt, max_len);

        if (!dev->rx_drop)
            memcpy(frame->dat + dev->rx_byte_cnt, rd, cpy_len);
        dev->rx_byte_cnt += cpy_len;

        if (dev->rx_byte_cnt == 3 && (frame->dat[2] > CD_FRAME_SIZE - 5 ||
                (dev->local_mac != 0xff && frame->dat[1] != 0xff && frame->dat[1] != dev->local_mac))) {
            dn_warn(dev->name, "drop, hdr: %02x %02x %02x\n", frame->dat[0], frame->dat[1], frame->dat[2]);
            dev->rx_drop = true;
        }

        if (!dev->rx_drop)
            dev->rx_crc = CDUART_CRC_SUB(rd, cpy_len, dev->rx_crc);
        rd += cpy_len;

        if (dev->rx_byte_cnt == frame->dat[2] + 5) {
            if (!dev->rx_drop) {
                if (dev->rx_crc != 0) {
                    dn_error(dev->name, "crc error, hdr: %02x %02x %02x\n",
                            frame->dat[0], frame->dat[1], frame->dat[2]);
                } else {
                    cd_frame_t *frm = cd_list_get(dev->free_head);
                    if (frm) {
#ifdef CD_VERBOSE
                        char pbuf[52];
                        hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
                        dn_verbose(dev->name, "-> [%s]\n", pbuf);
#endif
                        cd_list_put(&dev->rx_head, dev->rx_frame);
                        dev->rx_frame = frm;
                    } else {
                        // set rx_lost flag
                        dn_error(dev->name, "rx_lost\n");
                    }
                }
            }
            dev->rx_byte_cnt = 0;
            dev->rx_crc = 0xffff;
            dev->rx_drop = false;
        }
    }
}
