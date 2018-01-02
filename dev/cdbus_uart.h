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
#include "6lo.h"

#ifndef CDUART_IDLE_TIME
#define CDUART_IDLE_TIME     500
#endif


typedef struct cduart_intf {
    cd_intf_t           cd_intf;

    uart_t              *uart;

    list_head_t         *free_head;
    list_head_t         rx_head;
    list_head_t         tx_head;

    list_node_t         *rx_node;    // init: != NULL
    uint16_t            rx_byte_cnt;
    uint32_t            t_last;      // last receive time

    list_node_t         *tx_node;

    uint8_t             local_filter[8];
    uint8_t             remote_filter[8];
    uint8_t             local_filter_len;
    uint8_t             remote_filter_len;
} cduart_intf_t;


void cduart_intf_init(cduart_intf_t *intf, list_head_t *free_head,
    uart_t *uart);

// you can call rx and tx task respectively or call a single task
void cduart_rx_task(cduart_intf_t *intf, uint8_t val);
void cduart_tx_task(cduart_intf_t *intf);
void cduart_task(cduart_intf_t *intf);

#endif

