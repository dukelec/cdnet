/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDCTL_IT_H__
#define __CDCTL_IT_H__

#include "cdbus.h"
#include "cdctl_regs.h"

typedef enum {
    CDCTL_RST = 0,

    CDCTL_IDLE,
    CDCTL_WAIT_TX_CLEAN,
    CDCTL_RD_FLAG,
    CDCTL_REG_W,

    CDCTL_RX_HEADER,
    CDCTL_RX_BODY,
    CDCTL_TX_FRAME
} cdctl_state_t;

typedef struct {
    cd_dev_t        cd_dev;
    const char      *name;
    uint8_t         version;
    uint32_t        sysclk;
    bool            _clr_flag; // need manual clr flag if version < 0x0e

    cdctl_state_t   state;

    list_head_t     *free_head;
    list_head_t     rx_head;
    list_head_t     tx_head;

    cd_frame_t      *rx_frame;
    cd_frame_t      *tx_frame;
    bool            tx_wait_trigger;
    bool            tx_buf_clean_mask;

    uint8_t         buf[2];

    uint32_t        rx_cnt;
    uint32_t        tx_cnt;
    uint32_t        rx_lost_cnt;
    uint32_t        rx_error_cnt;
    uint32_t        rx_break_cnt;
    uint32_t        tx_cd_cnt;
    uint32_t        tx_error_cnt;
    uint32_t        rx_no_free_node_cnt;
    uint32_t        rx_len_err_cnt;

    spi_t           *spi;
    gpio_t          *rst_n;
    gpio_t          *int_n;
    irq_t           int_irq;
} cdctl_dev_t;

typedef struct {
    uint8_t         mac;
    uint32_t        baud_l;
    uint32_t        baud_h;
    uint8_t         filter_m[2];

    uint8_t         mode; // 0: Arbitration, 1: Break Sync
    uint16_t        tx_permit_len;
    uint16_t        max_idle_len;
    uint8_t         tx_pre_len;
} cdctl_cfg_t;

#define CDCTL_CFG_DFT(_mac) {   \
    .mac = _mac,                \
    .baud_l = 115200,           \
    .baud_h = 115200,           \
    .filter_m = { 0xff, 0xff }, \
    .mode = 0,                  \
    .tx_permit_len = 0x14,      \
    .max_idle_len = 0xc8,       \
    .tx_pre_len = 0x01          \
}

void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head, cdctl_cfg_t *init,
        spi_t *spi, gpio_t *rst_n, gpio_t *int_n, irq_t int_irq);

uint8_t cdctl_reg_r(cdctl_dev_t *dev, uint8_t reg);
void cdctl_reg_w(cdctl_dev_t *dev, uint8_t reg, uint8_t val);
void cdctl_set_baud_rate(cdctl_dev_t *dev, uint32_t low, uint32_t high);
void cdctl_get_baud_rate(cdctl_dev_t *dev, uint32_t *low, uint32_t *high);

cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev);
void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame);

static inline void cdctl_flush(cdctl_dev_t *dev)
{
    cdctl_reg_w(dev, REG_RX_CTRL, BIT_RX_RST_ALL);
}

void cdctl_int_isr(cdctl_dev_t *dev);
void cdctl_spi_isr(cdctl_dev_t *dev);

#endif
