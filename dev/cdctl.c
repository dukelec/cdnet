/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdctl.h"
#include "cd_debug.h"
#include "cdctl_pll_cal.h"


uint8_t cdctl_reg_r(cdctl_dev_t *dev, uint8_t reg)
{
    uint8_t dat = 0xff;
    spi_mem_read(dev->spi, reg, &dat, 1);
    return dat;
}

void cdctl_reg_w(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
    spi_mem_write(dev->spi, reg | 0x80, &val, 1);
}

static int cdctl_read_frame(cdctl_dev_t *dev, cd_frame_t *frame)
{
    spi_mem_read(dev->spi, REG_RX, frame->dat, 3);
    if (frame->dat[2] > min(CD_FRAME_SIZE - 3, 253))
        return -1;
    spi_mem_read(dev->spi, REG_RX, frame->dat + 3, frame->dat[2]);
    return 0;
}

static void cdctl_write_frame(cdctl_dev_t *dev, const cd_frame_t *frame)
{
    spi_mem_write(dev->spi, REG_TX | 0x80, frame->dat, frame->dat[2] + 3);
}

// member functions

cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return cd_list_get(&dev->rx_head);
}

void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    cd_list_put(&dev->tx_head, frame);
}

void cdctl_set_baud_rate(cdctl_dev_t *dev, uint32_t low, uint32_t high)
{
    uint16_t l, h;
    l = DIV_ROUND_CLOSEST(dev->sysclk, low) - 1;
    h = DIV_ROUND_CLOSEST(dev->sysclk, high) - 1;
    cdctl_reg_w(dev, REG_DIV_LS_L, l & 0xff);
    cdctl_reg_w(dev, REG_DIV_LS_H, l >> 8);
    cdctl_reg_w(dev, REG_DIV_HS_L, h & 0xff);
    cdctl_reg_w(dev, REG_DIV_HS_H, h >> 8);
    dn_debug(dev->name, "set baud rate: %u %u (%u %u)\n", low, high, l, h);
}

void cdctl_get_baud_rate(cdctl_dev_t *dev, uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    l = cdctl_reg_r(dev, REG_DIV_LS_L) | cdctl_reg_r(dev, REG_DIV_LS_H) << 8;
    h = cdctl_reg_r(dev, REG_DIV_HS_L) | cdctl_reg_r(dev, REG_DIV_HS_H) << 8;
    *low = DIV_ROUND_CLOSEST(dev->sysclk, l + 1);
    *high = DIV_ROUND_CLOSEST(dev->sysclk, h + 1);
}


void cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head, cdctl_cfg_t *init,
        spi_t *spi, gpio_t *rst_n)
{
    if (!dev->name)
        dev->name = "cdctl";
    dev->free_head = free_head;
    dev->cd_dev.get_rx_frame = cdctl_get_rx_frame;
    dev->cd_dev.put_tx_frame = cdctl_put_tx_frame;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->is_pending = false;
#endif

    dev->spi = spi;
    dev->rst_n = rst_n;

    dn_info(dev->name, "init...\n");
    if (rst_n) {
        gpio_set_low(rst_n);
        delay_systick(2000/SYSTICK_US_DIV);
        gpio_set_high(rst_n);
        delay_systick(2000/SYSTICK_US_DIV);
    }

    // the fpga has to be read multiple times, the asic does not
    uint8_t last_ver = 0xff;
    uint8_t same_cnt = 0;
    while (true) {
        uint8_t ver = cdctl_reg_r(dev, REG_VERSION);
        if (ver != 0x00 && ver != 0xff && ver == last_ver) {
            if (same_cnt++ > 10)
                break;
        } else {
            last_ver = ver;
            same_cnt = 0;
        }
        debug_flush(false);
    }
    dn_info(dev->name, "version: %02x\n", last_ver);
    dev->version = last_ver;
    dev->_clr_flag = last_ver >= 0x0e ? false : true;

    if (dev->version >= 0x10) { // asic
        cdctl_reg_w(dev, REG_CLK_CTRL, 0x80); // soft reset
        dn_info(dev->name, "version after soft reset: %02x\n", cdctl_reg_r(dev, REG_VERSION));
#ifndef CDCTL_AVOID_PIN_RE
        cdctl_reg_w(dev, REG_PIN_RE_CTRL, 0x10); // enable phy rx
#endif
        dev->sysclk = cdctl_sys_cal(init->baud_h);
        pllcfg_t pll = cdctl_pll_cal(CDCTL_OSC_CLK, dev->sysclk);
        uint32_t actual_freq = cdctl_pll_get(CDCTL_OSC_CLK, pll);
        d_info("cdctl: sysclk %ld, actual: %d\n", dev->sysclk, actual_freq);
        dev->sysclk = actual_freq;
        cdctl_reg_w(dev, REG_PLL_N, pll.n);
        cdctl_reg_w(dev, REG_PLL_ML, pll.m & 0xff);
        cdctl_reg_w(dev, REG_PLL_OD_MH, (pll.d << 4) | (pll.m >> 8));
        d_info("pll_n: %02x, ml: %02x, od_mh: %02x\n",
                cdctl_reg_r(dev, REG_PLL_N), cdctl_reg_r(dev, REG_PLL_ML), cdctl_reg_r(dev, REG_PLL_OD_MH));
        d_info("pll_ctrl: %02x\n", cdctl_reg_r(dev, REG_PLL_CTRL));
        cdctl_reg_w(dev, REG_PLL_CTRL, 0x10); // enable pll
        d_info("clk_status: %02x\n", cdctl_reg_r(dev, REG_CLK_STATUS));
        cdctl_reg_w(dev, REG_CLK_CTRL, 0x01); // select pll
        d_info("clk_status after select pll: %02x\n", cdctl_reg_r(dev, REG_CLK_STATUS));
        d_info("version after select pll: %02x\n", cdctl_reg_r(dev, REG_VERSION));
    } else {
        dev->sysclk = 40e6L;
        d_info("fallback to cdctl-b1 module, ver: %02x\n", dev->version);
    }

    uint8_t setting = (cdctl_reg_r(dev, REG_SETTING) & 0xf) | BIT_SETTING_TX_PUSH_PULL;
    setting |= init->mode == 1 ? BIT_SETTING_BREAK_SYNC : BIT_SETTING_ARBITRATE;
    cdctl_reg_w(dev, REG_SETTING, setting);
    cdctl_reg_w(dev, REG_FILTER, init->mac);
    cdctl_reg_w(dev, REG_FILTER_M0, init->filter_m[0]);
    cdctl_reg_w(dev, REG_FILTER_M1, init->filter_m[1]);
    cdctl_reg_w(dev, REG_TX_PERMIT_LEN_L, init->tx_permit_len & 0xff);
    cdctl_reg_w(dev, REG_TX_PERMIT_LEN_H, init->tx_permit_len >> 8);
    cdctl_reg_w(dev, REG_MAX_IDLE_LEN_L, init->max_idle_len & 0xff);
    cdctl_reg_w(dev, REG_MAX_IDLE_LEN_H, init->max_idle_len >> 8);
    cdctl_reg_w(dev, REG_TX_PRE_LEN, init->tx_pre_len);
    cdctl_set_baud_rate(dev, init->baud_l, init->baud_h);
    cdctl_flush(dev);

    cdctl_get_baud_rate(dev, &init->baud_l, &init->baud_h);
    d_debug("cdctl: get baud rate: %lu %lu\n", init->baud_l, init->baud_h);
    d_debug("cdctl: get filter(m): %02x (%02x %02x)\n",
            cdctl_reg_r(dev, REG_FILTER), cdctl_reg_r(dev, REG_FILTER_M0), cdctl_reg_r(dev, REG_FILTER_M1));
    dn_debug(dev->name, "flags: %02x\n", cdctl_reg_r(dev, REG_INT_FLAG));
}

// handlers


void cdctl_routine(cdctl_dev_t *dev)
{
    uint8_t flags = cdctl_reg_r(dev, REG_INT_FLAG);

    if (flags & (BIT_FLAG_RX_LOST | BIT_FLAG_RX_ERROR | BIT_FLAG_RX_BREAK | \
            BIT_FLAG_TX_CD | BIT_FLAG_TX_ERROR)) {
        if (flags & BIT_FLAG_RX_LOST) {
            dn_error(dev->name, "BIT_FLAG_RX_LOST\n");
            if (dev->_clr_flag)
                cdctl_reg_w(dev, REG_RX_CTRL, BIT_RX_CLR_LOST);
        }
        if (flags & BIT_FLAG_RX_ERROR) {
            dn_warn(dev->name, "BIT_FLAG_RX_ERROR\n");
            if (dev->_clr_flag)
                cdctl_reg_w(dev, REG_RX_CTRL, BIT_RX_CLR_ERROR);
        }
        if (flags & BIT_FLAG_TX_CD) {
            dn_debug(dev->name, "BIT_FLAG_TX_CD\n");
            if (dev->_clr_flag)
                cdctl_reg_w(dev, REG_TX_CTRL, BIT_TX_CLR_CD);
        }
        if (flags & BIT_FLAG_TX_ERROR) {
            dn_error(dev->name, "BIT_FLAG_TX_ERROR\n");
            if (dev->_clr_flag)
                cdctl_reg_w(dev, REG_TX_CTRL, BIT_TX_CLR_ERROR);
        }
    }

    if (flags & BIT_FLAG_RX_PENDING) {
        // if get free list: copy to rx list
        cd_frame_t *frame = cd_list_get(dev->free_head);
        if (frame) {
            int ret = cdctl_read_frame(dev, frame);
            cdctl_reg_w(dev, REG_RX_CTRL, BIT_RX_CLR_PENDING | BIT_RX_RST_POINTER);
#ifdef VERBOSE
            char pbuf[52];
            hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
            dn_verbose(dev->name, "-> [%s]\n", pbuf);
#endif
            if (ret) {
                dn_error(dev->name, "rx frame len err\n");
                cd_list_put(dev->free_head, frame);
            } else {
                cd_list_put(&dev->rx_head, frame);
            }
        } else {
            dn_error(dev->name, "get rx, no free frame\n");
        }
    }

    if (!dev->is_pending) {
        if (dev->tx_head.first) {
            cd_frame_t *frame = cd_list_get(&dev->tx_head);
            cdctl_write_frame(dev, frame);

            if (flags & BIT_FLAG_TX_BUF_CLEAN)
                cdctl_reg_w(dev, REG_TX_CTRL, BIT_TX_START | BIT_TX_RST_POINTER);
            else
                dev->is_pending = true;
#ifdef VERBOSE
            char pbuf[52];
            hex_dump_small(pbuf, frame->dat, frame->dat[2] + 3, 16);
            dn_verbose(dev->name, "<- [%s]%s\n", pbuf, dev->is_pending ? " (p)" : "");
#endif
            cd_list_put(dev->free_head, frame);
        }
    } else {
        if (flags & BIT_FLAG_TX_BUF_CLEAN) {
            dn_verbose(dev->name, "trigger pending tx\n");
            cdctl_reg_w(dev, REG_TX_CTRL, BIT_TX_START | BIT_TX_RST_POINTER);
            dev->is_pending = false;
        }
    }
}
