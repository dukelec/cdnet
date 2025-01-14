/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDBUS_UART_H__
#define __CDBUS_UART_H__

#include "cdbus.h"
#include "modbus_crc.h"

#ifndef CDUART_IDLE_TIME
#define CDUART_IDLE_TIME    (5000 / SYSTICK_US_DIV) // 5 ms
#endif
#ifndef CDUART_CRC
#define CDUART_CRC          crc16
#endif
#ifndef CDUART_CRC_SUB
#define CDUART_CRC_SUB      crc16_sub
#endif

typedef struct cduart_dev {
    cd_dev_t            cd_dev;
    const char          *name;

    list_head_t         *free_head;
    list_head_t         rx_head;
    list_head_t         tx_head;

    cd_frame_t          *rx_frame;  // init: != NULL
    uint16_t            rx_byte_cnt;
    uint16_t            rx_crc;
    bool                rx_drop;
    uint32_t            t_last;     // last receive time

    uint8_t             local_mac;
} cduart_dev_t;


void cduart_dev_init(cduart_dev_t *dev, list_head_t *free_head);
void cduart_rx_handle(cduart_dev_t *dev, const uint8_t *buf, unsigned len);

static inline void cduart_fill_crc(uint8_t *dat)
{
    uint16_t crc_val = CDUART_CRC(dat, dat[2] + 3);
    dat[dat[2] + 3] = crc_val & 0xff;
    dat[dat[2] + 4] = crc_val >> 8;
}

#endif
