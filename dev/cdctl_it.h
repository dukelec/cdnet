/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDCTL_IT_H__
#define __CDCTL_IT_H__

#include "cdbus.h"
#ifdef CDCTL_V0X
#include "cdctl_regs_v0x.h"
#else // >= v10
#include "cdctl_regs.h"
#endif

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
    cd_dev_t        cd_dev;
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
} cdctl_dev_t;


void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
        spi_t *spi, gpio_t *rst_n, gpio_t *int_n);

uint8_t cdctl_read_reg(cdctl_dev_t *dev, uint8_t reg);
void cdctl_write_reg(cdctl_dev_t *dev, uint8_t reg, uint8_t val);
void cdctl_set_baud_rate(cdctl_dev_t *dev, uint32_t low, uint32_t high);
void cdctl_get_baud_rate(cdctl_dev_t *dev, uint32_t *low, uint32_t *high);

cd_frame_t *cdctl_get_free_frame(cd_dev_t *cd_dev);
cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev);
void cdctl_put_free_frame(cd_dev_t *cd_dev, cd_frame_t *frame);
void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame);

static inline void cdctl_flush(cdctl_dev_t *dev)
{
    cdctl_write_reg(dev, REG_RX_CTRL, BIT_RX_RST_ALL);
}

void cdctl_int_isr(cdctl_dev_t *dev);
void cdctl_spi_isr(cdctl_dev_t *dev);

#endif
