/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDCTL_H__
#define __CDCTL_H__

#include "cdbus.h"
#include "cdctl_regs.h"

typedef struct {
    cd_dev_t    cd_dev;
    const char  *name;
    uint8_t     version;
    bool        _clr_flag; // need manual clr flag if version < 0x0e

    list_head_t *free_head;
    list_head_t rx_head;
    list_head_t tx_head;

    bool        is_pending;

#ifdef CDCTL_I2C
    i2c_t       *i2c;
#else
    spi_t       *spi;
#endif
    gpio_t      *rst_n;
} cdctl_dev_t;

typedef struct {
    uint8_t         mac;
    uint32_t        baud_l;
    uint32_t        baud_h;
    uint8_t         filter[2];

    uint8_t         mode; // 0: Arbitration, 1: Break Sync
    uint16_t        tx_permit_len;
    uint16_t        max_idle_len;
    uint8_t         tx_pre_len;
} cdctl_cfg_t;

#define CDCTL_CFG_DFT(_mac) {   \
    .mac = _mac,                \
    .baud_l = 115200,           \
    .baud_h = 115200,           \
    .filter = { 0xff, 0xff },   \
    .mode = 0,                  \
    .tx_permit_len = 0x14,      \
    .max_idle_len = 0xc8,       \
    .tx_pre_len = 0x01          \
}


#ifdef CDCTL_I2C
void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head, cdctl_cfg_t *init,
        i2c_t *i2c, gpio_t *rst_n);
#else
void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head, cdctl_cfg_t *init,
        spi_t *spi, gpio_t *rst_n);
#endif

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

void cdctl_routine(cdctl_dev_t *dev);

#endif

