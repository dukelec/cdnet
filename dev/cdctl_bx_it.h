/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDCTL_BX_IT_H__
#define __CDCTL_BX_IT_H__

#include "cdnet.h"

typedef enum {
    CDCTL_RST = 0,

    CDCTL_IDLE,
    CDCTL_WAIT_TX_CLEAN,
    CDCTL_RD_FLAG,

    CDCTL_RX_HEADER,
    CDCTL_RX_BODY,
    CDCTL_RX_CTRL,

    CDCTL_TX_HEADER,
    CDCTL_TX_BODY,
    CDCTL_TX_CTRL,
    CDCTL_TX_MASK
} cdctl_state_t;

typedef struct {
    cd_intf_t       cd_intf;
    const char      *name;

    cdctl_state_t   state;
    bool            manual_ctrl;

    list_head_t     *free_head;
    list_head_t     rx_head;
    list_head_t     tx_head;

    cd_frame_t      *rx_frame;
    bool            tx_wait_trigger;
    bool            tx_buf_clean_mask;

    uint8_t         buf[4];

    uint32_t        rx_cnt;
    uint32_t        tx_cnt;
    uint32_t        rx_lost_cnt;
    uint32_t        rx_error_cnt;
    uint32_t        tx_cd_cnt;
    uint32_t        tx_error_cnt;
    uint32_t        rx_no_free_node_cnt;

    spi_t           *spi;
    gpio_t          *rst_n;
    gpio_t          *int_n;
} cdctl_intf_t;


void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
        spi_t *spi, gpio_t *rst_n, gpio_t *int_n);

cd_frame_t *cdctl_get_free_frame(cd_intf_t *cd_intf);
cd_frame_t *cdctl_get_rx_frame(cd_intf_t *cd_intf);
void cdctl_put_free_frame(cd_intf_t *cd_intf, cd_frame_t *frame);
void cdctl_put_tx_frame(cd_intf_t *cd_intf, cd_frame_t *frame);

void cdctl_int_isr(cdctl_intf_t *intf);
void cdctl_spi_isr(cdctl_intf_t *intf);

#endif
