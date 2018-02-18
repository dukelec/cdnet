/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDBUS_UART_H__
#define __CDBUS_UART_H__

#include "common.h"
#include "modbus_crc.h"
#include "cdnet.h"

#ifndef CDUART_IDLE_TIME
#define CDUART_IDLE_TIME    (5000 / SYSTICK_US_DIV) // 5 ms
#endif

typedef struct cduart_intf {
    cd_intf_t           cd_intf;

    list_head_t         *free_head;
    list_head_t         rx_head;
    list_head_t         tx_head;

    list_node_t         *rx_node;    // init: != NULL
    uint16_t            rx_byte_cnt;
    uint16_t            rx_crc;
    uint32_t            t_last;      // last receive time

    uint8_t             local_filter[8];
    uint8_t             remote_filter[8];
    uint8_t             local_filter_len;
    uint8_t             remote_filter_len;
} cduart_intf_t;


void cduart_intf_init(cduart_intf_t *intf, list_head_t *free_head);
void cduart_rx_handle(cduart_intf_t *intf, const uint8_t *buf, int len);

static inline void cduart_fill_crc(uint8_t *dat)
{
    uint16_t crc_val = crc16(dat, dat[2] + 3);
    dat[dat[2] + 3] = crc_val & 0xff;
    dat[dat[2] + 4] = crc_val >> 8;
}

#endif
