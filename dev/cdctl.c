/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdctl.h"


uint8_t cdctl_read_reg(cdctl_dev_t *dev, uint8_t reg)
{
    uint8_t dat = 0xff;
#ifdef CDCTL_I2C
    i2c_mem_read(dev->i2c, reg, &dat, 1);
#else
    spi_mem_read(dev->spi, reg, &dat, 1);
#endif
    return dat;
}

void cdctl_write_reg(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
#ifdef CDCTL_I2C
    i2c_mem_write(dev->i2c, reg, &val, 1);
#else
    spi_mem_write(dev->spi, reg | 0x80, &val, 1);
#endif
}

static void cdctl_read_frame(cdctl_dev_t *dev, cd_frame_t *frame)
{
#ifdef CDCTL_I2C
    i2c_mem_read(dev->i2c, REG_RX, frame->dat, 3);
    i2c_mem_read(dev->i2c, REG_RX, frame->dat + 3, frame->dat[2]);
#else
    spi_mem_read(dev->spi, REG_RX, frame->dat, 3);
    spi_mem_read(dev->spi, REG_RX, frame->dat + 3, frame->dat[2]);
#endif
}

static void cdctl_write_frame(cdctl_dev_t *dev, const cd_frame_t *frame)
{
#ifdef CDCTL_I2C
    i2c_mem_write(dev->i2c, REG_TX, frame->dat, frame->dat[2] + 3);
#else
    spi_mem_write(dev->spi, REG_TX | 0x80, frame->dat, frame->dat[2] + 3);
#endif
}

// member functions

cd_frame_t *cdctl_get_free_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return list_get_entry(dev->free_head, cd_frame_t);
}

cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return list_get_entry(&dev->rx_head, cd_frame_t);
}

void cdctl_put_free_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    list_put(dev->free_head, &frame->node);
}

void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    list_put(&dev->tx_head, &frame->node);
}

void cdctl_set_baud_rate(cdctl_dev_t *dev, uint32_t low, uint32_t high)
{
    uint16_t l, h;
    l = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, low) - 1;
    h = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, high) - 1;
    cdctl_write_reg(dev, REG_DIV_LS_L, l & 0xff);
    cdctl_write_reg(dev, REG_DIV_LS_H, l >> 8);
    cdctl_write_reg(dev, REG_DIV_HS_L, h & 0xff);
    cdctl_write_reg(dev, REG_DIV_HS_H, h >> 8);
    dn_debug(dev->name, "set baud rate: %u %u (%u %u)\n", low, high, l, h);
}

void cdctl_get_baud_rate(cdctl_dev_t *dev, uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    l = cdctl_read_reg(dev, REG_DIV_LS_L) |
            cdctl_read_reg(dev, REG_DIV_LS_H) << 8;
    h = cdctl_read_reg(dev, REG_DIV_HS_L) |
            cdctl_read_reg(dev, REG_DIV_HS_H) << 8;
    *low = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, l + 1);
    *high = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, h + 1);
}

void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
#ifdef CDCTL_I2C
        i2c_t *i2c, gpio_t *rst_n)
#else
        spi_t *spi, gpio_t *rst_n)
#endif
{
    if (!dev->name)
        dev->name = "cdctl";
    dev->free_head = free_head;
    dev->cd_dev.get_free_frame = cdctl_get_free_frame;
    dev->cd_dev.get_rx_frame = cdctl_get_rx_frame;
    dev->cd_dev.put_free_frame = cdctl_put_free_frame;
    dev->cd_dev.put_tx_frame = cdctl_put_tx_frame;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->is_pending = false;
#endif

#ifdef CDCTL_I2C
    dev->i2c = i2c;
#else
    dev->spi = spi;
#endif
    dev->rst_n = rst_n;

    dn_info(dev->name, "init...\n");
    if (rst_n) {
        gpio_set_value(rst_n, 0);
        delay_systick(2000/SYSTICK_US_DIV);
        gpio_set_value(rst_n, 1);
        delay_systick(2000/SYSTICK_US_DIV);
    }

    uint8_t last_ver = 0xff;
    uint8_t same_cnt = 0;
    while (true) {
        uint8_t ver = cdctl_read_reg(dev, REG_VERSION);
        if (ver != 0x00 && ver != 0xff && ver == last_ver) {
            if (same_cnt++ > 10)
                break;
        } else {
            last_ver = ver;
            same_cnt = 0;
        }
        debug_flush();
    }
    dn_info(dev->name, "version: %02x\n", last_ver);

    cdctl_write_reg(dev, REG_SETTING, BIT_SETTING_TX_PUSH_PULL);
    cdctl_write_reg(dev, REG_FILTER, filter);
    cdctl_set_baud_rate(dev, baud_l, baud_h);
    cdctl_flush(dev);

    dn_debug(dev->name, "flags: %02x\n", cdctl_read_reg(dev, REG_INT_FLAG));
}

// handlers


void cdctl_routine(cdctl_dev_t *dev)
{
    uint8_t flags = cdctl_read_reg(dev, REG_INT_FLAG);

    if (flags & BIT_FLAG_RX_LOST) {
        dn_error(dev->name, "BIT_FLAG_RX_LOST\n");
        cdctl_write_reg(dev, REG_RX_CTRL, BIT_RX_CLR_LOST);
    }
    if (flags & BIT_FLAG_RX_ERROR) {
        dn_warn(dev->name, "BIT_FLAG_RX_ERROR\n");
        cdctl_write_reg(dev, REG_RX_CTRL, BIT_RX_CLR_ERROR);
    }
    if (flags & BIT_FLAG_TX_CD) {
        dn_debug(dev->name, "BIT_FLAG_TX_CD\n");
        cdctl_write_reg(dev, REG_TX_CTRL, BIT_TX_CLR_CD);
    }
    if (flags & BIT_FLAG_TX_ERROR) {
        dn_error(dev->name, "BIT_FLAG_TX_ERROR\n");
        cdctl_write_reg(dev, REG_TX_CTRL, BIT_TX_CLR_ERROR);
    }

    if (flags & BIT_FLAG_RX_PENDING) {
        // if get free list: copy to rx list
        cd_frame_t *frame = list_get_entry(dev->free_head, cd_frame_t);
        if (frame) {
            cdctl_read_frame(dev, frame);
            cdctl_write_reg(dev, REG_RX_CTRL, BIT_RX_CLR_PENDING);
#ifdef VERBOSE
            char pbuf[52];
            hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
            dn_verbose(dev->name, "-> [%s]\n", pbuf);
#endif
            list_put(&dev->rx_head, &frame->node);
        } else {
            dn_error(dev->name, "get_rx, no free frame\n");
        }
    }

    if (!dev->is_pending && dev->tx_head.first) {
        cd_frame_t *frame = list_get_entry(&dev->tx_head, cd_frame_t);
        cdctl_write_frame(dev, frame);

        flags = cdctl_read_reg(dev, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN)
            cdctl_write_reg(dev, REG_TX_CTRL, BIT_TX_START);
        else
            dev->is_pending = true;
#ifdef VERBOSE
        char pbuf[52];
        hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
        dn_verbose(dev->name, "<- [%s]%s\n",
                pbuf, dev->is_pending ? " (p)" : "");
#endif
        list_put(dev->free_head, &frame->node);
    }

    if (dev->is_pending) {
        flags = cdctl_read_reg(dev, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN) {
            dn_verbose(dev->name, "trigger pending tx\n");
            cdctl_write_reg(dev, REG_TX_CTRL, BIT_TX_START);
            dev->is_pending = false;
        }
    }
}
