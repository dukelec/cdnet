/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

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

static cd_frame_t *cdctl_get_free_frame(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get_entry(intf->free_head, cd_frame_t);
}

static cd_frame_t *cdctl_get_rx_frame(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get_entry(&intf->rx_head, cd_frame_t);
}

static void cdctl_put_free_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    list_put(intf->free_head, &frame->node);
}

static void cdctl_put_tx_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    list_put(&intf->tx_head, &frame->node);
}

static void cdctl_set_filter(cd_intf_t *cd_intf, uint8_t filter)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    cdctl_write_reg(intf, REG_FILTER, filter);
}

static uint8_t cdctl_get_filter(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return cdctl_read_reg(intf, REG_FILTER);
}

static void cdctl_set_tx_wait(cd_intf_t *cd_intf, uint8_t len)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    cdctl_write_reg(intf, REG_TX_WAIT_LEN, max(1, len));
}

static uint8_t cdctl_get_tx_wait(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return cdctl_read_reg(intf, REG_TX_WAIT_LEN);
}

static void cdctl_set_baud_rate(cd_intf_t *cd_intf,
        uint32_t low, uint32_t high)
{
    uint16_t l, h;
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    l = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, low) - 1;
    h = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, high) - 1;
    cdctl_write_reg(intf, REG_DIV_LS_L, l & 0xff);
    cdctl_write_reg(intf, REG_DIV_LS_H, l >> 8);
    cdctl_write_reg(intf, REG_DIV_HS_L, h & 0xff);
    cdctl_write_reg(intf, REG_DIV_HS_H, h >> 8);
    dn_debug(intf->name, "set baud rate: %u %u (%u %u)\n", low, high, l, h);
}

static void cdctl_get_baud_rate(cd_intf_t *cd_intf,
        uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    l = cdctl_read_reg(intf, REG_DIV_LS_L) |
            cdctl_read_reg(intf, REG_DIV_LS_H) << 8;
    h = cdctl_read_reg(intf, REG_DIV_HS_L) |
            cdctl_read_reg(intf, REG_DIV_HS_H) << 8;
    *low = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, l + 1);
    *high = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, h + 1);
}

static void cdctl_flush(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_RST);
}

void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
#ifdef CDCTL_I2C
        i2c_t *i2c, gpio_t *rst_n)
#else
        spi_t *spi, gpio_t *rst_n)
#endif
{
    if (!intf->name)
        intf->name = "cdctl";
    intf->free_head = free_head;
    intf->cd_intf.get_free_frame = cdctl_get_free_frame;
    intf->cd_intf.get_rx_frame = cdctl_get_rx_frame;
    intf->cd_intf.put_free_frame = cdctl_put_free_frame;
    intf->cd_intf.put_tx_frame = cdctl_put_tx_frame;
    intf->cd_intf.set_filter = cdctl_set_filter;
    intf->cd_intf.get_filter = cdctl_get_filter;
    intf->cd_intf.set_tx_wait = cdctl_set_tx_wait;
    intf->cd_intf.get_tx_wait = cdctl_get_tx_wait;
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

    dn_info(intf->name, "init...\n");
    if (rst_n) {
        gpio_set_value(rst_n, 0);
        delay_systick(2000/SYSTICK_US_DIV);
        gpio_set_value(rst_n, 1);
        delay_systick(2000/SYSTICK_US_DIV);
    }

    uint8_t last_ver = 0xff;
    uint8_t same_cnt = 0;
    while (true) {
        uint8_t ver = cdctl_read_reg(intf, REG_VERSION);
        if (ver != 0x00 && ver != 0xff && ver == last_ver) {
            if (same_cnt++ > 10)
                break;
        } else {
            last_ver = ver;
            same_cnt = 0;
        }
        debug_flush();
    }
    dn_info(intf->name, "version: %02x\n", last_ver);

    cdctl_write_reg(intf, REG_SETTING, BIT_SETTING_TX_PUSH_PULL);
    cdctl_set_filter(&intf->cd_intf, filter);
    cdctl_set_baud_rate(&intf->cd_intf, baud_l, baud_h);
    cdctl_flush(&intf->cd_intf);

    dn_debug(intf->name, "flags: %02x\n", cdctl_read_reg(intf, REG_INT_FLAG));
}

// handlers


void cdctl_routine(cdctl_intf_t *intf)
{
    uint8_t flags = cdctl_read_reg(intf, REG_INT_FLAG);

    if (flags & BIT_FLAG_RX_LOST) {
        dn_error(intf->name, "BIT_FLAG_RX_LOST\n");
        cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_LOST);
    }
    if (flags & BIT_FLAG_RX_ERROR) {
        dn_warn(intf->name, "BIT_FLAG_RX_ERROR\n");
        cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_ERROR);
    }
    if (flags & BIT_FLAG_TX_CD) {
        dn_debug(intf->name, "BIT_FLAG_TX_CD\n");
        cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_CLR_CD);
    }
    if (flags & BIT_FLAG_TX_ERROR) {
        dn_error(intf->name, "BIT_FLAG_TX_ERROR\n");
        cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_CLR_ERROR);
    }

    if (flags & BIT_FLAG_RX_PENDING) {
        // if get free list: copy to rx list
        cd_frame_t *frame = list_get_entry(intf->free_head, cd_frame_t);
        if (frame) {
            cdctl_read_frame(intf, frame);
            cdctl_write_reg(intf, REG_RX_CTRL, BIT_RX_CLR_PENDING);
#ifdef VERBOSE
            char pbuf[52];
            hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
            dn_verbose(intf->name, "-> [%s]\n", pbuf);
#endif
            list_put(&intf->rx_head, &frame->node);
        } else {
            dn_error(intf->name, "get_rx, no free frame\n");
        }
    }

    if (!intf->is_pending && intf->tx_head.first) {
        cd_frame_t *frame = list_get_entry(&intf->tx_head, cd_frame_t);
        cdctl_write_frame(intf, frame);

        flags = cdctl_read_reg(intf, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN)
            cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_START);
        else
            intf->is_pending = true;
#ifdef VERBOSE
        char pbuf[52];
        hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
        dn_verbose(intf->name, "<- [%s]%s\n",
                pbuf, intf->is_pending ? " (p)" : "");
#endif
        list_put(intf->free_head, &frame->node);
    }

    if (intf->is_pending) {
        flags = cdctl_read_reg(intf, REG_INT_FLAG);
        if (flags & BIT_FLAG_TX_BUF_CLEAN) {
            dn_verbose(intf->name, "trigger pending tx\n");
            cdctl_write_reg(intf, REG_TX_CTRL, BIT_TX_START);
            intf->is_pending = false;
        }
    }
}
