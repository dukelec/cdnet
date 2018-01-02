/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDBUS_BYTEWISE_H__
#define __CDBUS_BYTEWISE_H__

#include "common.h"
#include "6lo.h"

#ifndef CDBW_IDLE_CNT
#define CDBW_IDLE_CNT     10
#endif
#ifndef CDBW_TX_CNT_RANGE
#define CDBW_TX_CNT_RANGE 100
#endif


typedef enum {
    RX_WAIT_IDLE = 0,
    RX_WAIT_DATA,
    RX_BUSY
} rx_state_t;

typedef enum {
    TX_WAIT_DATA = 0,
    TX_KEEP_IDLE,
    TX_BUSY
} tx_state_t;

typedef struct {
    cd_intf_t   cd_intf;
    list_head_t *free_head;
    list_head_t rx_head;
    list_head_t tx_head;

    int         time_cnt;
    uart_t      *uart;
    gpio_t      *rx_pin;

    rx_state_t  rx_state;
    list_node_t *rx_node; // init: != NULL
    uint16_t    rx_crc;
    uint16_t    rx_byte_cnt;
    uint8_t     rx_filter;

    tx_state_t  tx_state;
    int         tx_time_wait;
    list_node_t *tx_node;
    uint16_t    tx_crc;
    uint16_t    tx_byte_cnt;
    uint8_t     tx_cd_cnt;
} cdbw_intf_t;


void cdbw_intf_init(cdbw_intf_t *intf, list_head_t *free_head,
    uart_t *uart, gpio_t *rx_pin);

void cdbw_rx_pin_irq_handler(cdbw_intf_t *intf);
void cdbw_timer_handler(cdbw_intf_t *intf);
void cdbw_rx_cplt_handler(cdbw_intf_t *intf);
void cdbw_tx_cplt_handler(cdbw_intf_t *intf);

#endif

