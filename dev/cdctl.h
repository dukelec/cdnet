/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDCTL_H__
#define __CDCTL_H__

#include "cdnet.h"
#include "cdctl_regs.h"

typedef struct {
    cd_intf_t   cd_intf;
    const char  *name;

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
} cdctl_intf_t;


#ifdef CDCTL_I2C
void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
        i2c_t *i2c, gpio_t *rst_n);
#else
void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
        spi_t *spi, gpio_t *rst_n);
#endif

uint8_t cdctl_read_reg(cdctl_intf_t *intf, uint8_t reg);
void cdctl_write_reg(cdctl_intf_t *intf, uint8_t reg, uint8_t val);
void cdctl_set_baud_rate(cdctl_intf_t *intf, uint32_t low, uint32_t high);
void cdctl_get_baud_rate(cdctl_intf_t *intf, uint32_t *low, uint32_t *high);

cd_frame_t *cdctl_get_free_frame(cd_intf_t *cd_intf);
cd_frame_t *cdctl_get_rx_frame(cd_intf_t *cd_intf);
void cdctl_put_free_frame(cd_intf_t *cd_intf, cd_frame_t *frame);
void cdctl_put_tx_frame(cd_intf_t *cd_intf, cd_frame_t *frame);

static inline void cdctl_flush(cdctl_intf_t *intf)
{
    cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_RST);
}

void cdctl_routine(cdctl_intf_t *intf);

#endif

