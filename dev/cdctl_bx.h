/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDCTL_BX_H__
#define __CDCTL_BX_H__

#include "cdnet.h"

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

void cdctl_task(cdctl_intf_t *intf);

#endif

