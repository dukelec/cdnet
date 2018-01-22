/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "common.h"
#include "cdctl_bx.h"
#include "cdctl_bx_regs.h"


static uint8_t cdctl_read_reg(cdctl_intf_t *intf, uint8_t reg)
{
    uint8_t dat = 0xff;
#ifdef CDCTL_I2C
    i2c_mem_read(intf->i2c, reg, &dat, 1);
#else
    spi_mem_read(intf->spi, reg, &dat, 1);
#endif
    return dat;
}

static void cdctl_write_reg(cdctl_intf_t *intf, uint8_t reg, uint8_t val)
{
#ifdef CDCTL_I2C
    i2c_mem_write(intf->i2c, reg, &val, 1);
#else
    spi_mem_write(intf->spi, reg | 0x80, &val, 1);
#endif
}

static void cdctl_read_frame(cdctl_intf_t *intf, cd_frame_t *frame)
{
#ifdef CDCTL_I2C
    i2c_mem_read(intf->i2c, REG_RX, frame->dat, 3);
    i2c_mem_read(intf->i2c, REG_RX, frame->dat + 3, frame->dat[2]);
#else
    spi_mem_read(intf->spi, REG_RX, frame->dat, 3);
    spi_mem_read(intf->spi, REG_RX, frame->dat + 3, frame->dat[2]);
#endif
}

static void cdctl_write_frame(cdctl_intf_t *intf, const cd_frame_t *frame)
{
#ifdef CDCTL_I2C
    i2c_mem_write(intf->i2c, REG_TX, frame->dat, frame->dat[2] + 3);
#else
    spi_mem_write(intf->spi, REG_TX | 0x80, frame->dat, frame->dat[2] + 3);
#endif
}

// member functions

static list_node_t *cdctl_get_free_node(cd_intf_t *cd_intf)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get(cdctl_intf->free_head);
}

static list_node_t *cdctl_get_rx_node(cd_intf_t *cd_intf)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get(&cdctl_intf->rx_head);
}

static void cdctl_put_free_node(cd_intf_t *cd_intf, list_node_t *node)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    list_put(cdctl_intf->free_head, node);
}

static void cdctl_put_tx_node(cd_intf_t *cd_intf, list_node_t *node)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    list_put(&cdctl_intf->tx_head, node);
}

static void cdctl_set_filter(cd_intf_t *cd_intf, uint8_t filter)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    cdctl_write_reg(cdctl_intf, REG_FILTER, filter);
    cdctl_write_reg(cdctl_intf, REG_TX_WAIT_LEN,
            filter == 255 ? 255 : filter + 1);
}

static uint8_t cdctl_get_filter(cd_intf_t *cd_intf)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return cdctl_read_reg(cdctl_intf, REG_FILTER);
}

static void cdctl_set_baud_rate(cd_intf_t *cd_intf,
        uint32_t low, uint32_t high)
{
    uint16_t l, h;
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    l = ((float)CDCTL_SYS_CLK / low) - 1 + 0.5;
    h = ((float)CDCTL_SYS_CLK / high) - 1 + 0.5;
    cdctl_write_reg(cdctl_intf, REG_DIV_LS_L, l & 0xff);
    cdctl_write_reg(cdctl_intf, REG_DIV_LS_H, l >> 8);
    cdctl_write_reg(cdctl_intf, REG_DIV_HS_L, h & 0xff);
    cdctl_write_reg(cdctl_intf, REG_DIV_HS_H, h >> 8);
    d_debug("cdctl %p: set baud rate: %u %u (%u %u)\n",
            cd_intf, low, high, l, h);
}

static void cdctl_get_baud_rate(cd_intf_t *cd_intf,
        uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    l = cdctl_read_reg(cdctl_intf, REG_DIV_LS_L) |
            cdctl_read_reg(cdctl_intf, REG_DIV_LS_H) << 8;
    h = cdctl_read_reg(cdctl_intf, REG_DIV_HS_L) |
            cdctl_read_reg(cdctl_intf, REG_DIV_HS_H) << 8;
    *low = ((float)CDCTL_SYS_CLK / (l + 1)) + 0.5;
    *high = ((float)CDCTL_SYS_CLK / (h + 1)) + 0.5;
}

static void cdctl_flush(cd_intf_t *cd_intf)
{
    cdctl_intf_t *cdctl_intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    cdctl_write_reg(cdctl_intf, REG_RX_CTRL, BIT_RX_RST);
}

void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
#ifdef CDCTL_I2C
        i2c_t *i2c, gpio_t *rst_n)
#else
        spi_t *spi, gpio_t *rst_n)
#endif
{
    intf->free_head = free_head;
    intf->cd_intf.get_free_node = cdctl_get_free_node;
    intf->cd_intf.get_rx_node = cdctl_get_rx_node;
    intf->cd_intf.put_free_node = cdctl_put_free_node;
    intf->cd_intf.put_tx_node = cdctl_put_tx_node;
    intf->cd_intf.set_filter = cdctl_set_filter;
    intf->cd_intf.get_filter = cdctl_get_filter;
    intf->cd_intf.set_baud_rate = cdctl_set_baud_rate;
    intf->cd_intf.get_baud_rate = cdctl_get_baud_rate;
    intf->cd_intf.flush = cdctl_flush;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    intf->is_pending = false;
#endif

#ifdef CDCTL_I2C
    intf->i2c = i2c;
#else
    intf->spi = spi;
#endif
    intf->rst_n = rst_n;

    if (rst_n) {
        gpio_set_value(rst_n, 0);
        gpio_set_value(rst_n, 1);
    }

    while (true) {
        uint8_t ver = cdctl_read_reg(intf, REG_VERSION);
        if (ver != 0xff && ver != 0x00 &&
                ver == cdctl_read_reg(intf, REG_VERSION)) {
            d_info("cdctl %p: version: %02x\n", intf, ver);
            break;
        }
        d_info("cdctl %p: not ready, ver: %02x\n", intf, ver);
        debug_flush();
    }

    cdctl_write_reg(intf, REG_SETTING, BIT_SETTING_TX_PUSH_PULL);
    cdctl_set_filter(&intf->cd_intf, filter);
    cdctl_set_baud_rate(&intf->cd_intf, baud_l, baud_h);
    cdctl_flush(&intf->cd_intf);

    d_debug("cdctl %p: flags: %02x\n", intf,
            cdctl_read_reg(intf, REG_INT_FLAG));
}

// handlers


void cdctl_task(cdctl_intf_t *intf)
{
    uint8_t flags = cdctl_read_reg(intf, REG_INT_FLAG);

    if (flags & BIT_FLAG_RX_LOST) {
        d_error("cdctl %p: BIT_FLAG_RX_LOST\n", intf);
        cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_LOST);
    }
    if (flags & BIT_FLAG_RX_ERROR) {
        d_warn("cdctl %p: BIT_FLAG_RX_ERROR\n", intf);
        cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_ERROR);
    }
    if (flags & BIT_FLAG_TX_CD) {
        d_debug("cdctl %p: BIT_FLAG_TX_CD\n", intf);
        cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_CLR_CD);
    }
    if (flags & BIT_FLAG_TX_ERROR) {
        d_error("cdctl %p: BIT_FLAG_TX_ERROR\n", intf);
        cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_CLR_ERROR);
    }

    if (flags & BIT_FLAG_RX_PENDING) {
        // if get free list: copy to rx list
        list_node_t *node = list_get(intf->free_head);
        if (node) {
            d_verbose("cdctl %p: get_rx\n", intf);
            cd_frame_t *frame = container_of(node, cd_frame_t, node);
            cdctl_read_frame(intf, frame);
            cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_PENDING);
            list_put(&intf->rx_head, node);
        } else {
            d_error("cdctl %p: get_rx, no free node\n", intf);
        }
    }

    if (!intf->is_pending && intf->tx_head.first) {
        list_node_t *node = list_get(&intf->tx_head);
        cd_frame_t *frame = container_of(node, cd_frame_t, node);
        d_verbose("cdctl %p: write frame\n", intf);
        cdctl_write_frame(intf, frame);

        flags = cdctl_read_reg(intf, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN) {
            d_verbose("cdctl %p: trigger tx\n", intf);
            cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_START);
        } else {
            intf->is_pending = true;
            d_verbose("cdctl %p: pending\n", intf);
        }
        list_put(intf->free_head, node);
    }

    if (intf->is_pending) {
        flags = cdctl_read_reg(intf, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN) {
            d_verbose("cdctl %p: trigger pending tx\n", intf);
            cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_START);
            intf->is_pending = false;
        }
    }
}
