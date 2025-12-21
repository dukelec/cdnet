/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdctl_it.h"
#include "cd_debug.h"
#include "cdctl_pll_cal.h"

#define CDCTL_MASK (CDBIT_FLAG_RX_PENDING | CDBIT_FLAG_RX_LOST | CDBIT_FLAG_RX_ERROR |  \
                    CDBIT_FLAG_TX_CD | CDBIT_FLAG_TX_ERROR)


uint8_t cdctl_reg_r(cdctl_dev_t *dev, uint8_t reg)
{
    uint8_t dat = 0xff;
    irq_disable(dev->int_irq);
    while (dev->state > CDCTL_WAIT_TX_CLEAN);
    spi_mem_read(dev->spi, reg, &dat, 1);
    irq_enable(dev->int_irq);
    return dat;
}

void cdctl_reg_w(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
    irq_disable(dev->int_irq);
    while (dev->state > CDCTL_WAIT_TX_CLEAN);
    spi_mem_write(dev->spi, reg | 0x80, &val, 1);
    irq_enable(dev->int_irq);
}


cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return cd_list_get(&dev->rx_head);
}

void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);

    cd_list_put(&dev->tx_head, frame);
    irq_disable(dev->int_irq);
    if (dev->state == CDCTL_IDLE)
        cdctl_int_isr(dev);
    irq_enable(dev->int_irq);
}


void cdctl_set_baud_rate(cdctl_dev_t *dev, uint32_t low, uint32_t high)
{
    uint16_t l, h;
    l = min(65535, max(2, DIV_ROUND_CLOSEST(dev->sysclk, low) - 1));
    h = min(65535, max(2, DIV_ROUND_CLOSEST(dev->sysclk, high) - 1));
    cdctl_reg_w(dev, CDREG_DIV_LS_L, l & 0xff);
    cdctl_reg_w(dev, CDREG_DIV_LS_H, l >> 8);
    cdctl_reg_w(dev, CDREG_DIV_HS_L, h & 0xff);
    cdctl_reg_w(dev, CDREG_DIV_HS_H, h >> 8);
    dn_debug(dev->name, "set baud rate: %"PRIu32" %"PRIu32" (%u %u)\n", low, high, l, h);
}

void cdctl_get_baud_rate(cdctl_dev_t *dev, uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    l = cdctl_reg_r(dev, CDREG_DIV_LS_L) | cdctl_reg_r(dev, CDREG_DIV_LS_H) << 8;
    h = cdctl_reg_r(dev, CDREG_DIV_HS_L) | cdctl_reg_r(dev, CDREG_DIV_HS_H) << 8;
    *low = DIV_ROUND_CLOSEST(dev->sysclk, l + 1);
    *high = DIV_ROUND_CLOSEST(dev->sysclk, h + 1);
}

void cdctl_set_clk(cdctl_dev_t *dev, uint32_t target_baud)
{
    cdctl_reg_w(dev, CDREG_CLK_CTRL, 0x00); // select osc
    while (!(cdctl_reg_r(dev, CDREG_CLK_STATUS) & 0b100)) {}
    dn_info(dev->name, "version (clk: osc): %02x\n", cdctl_reg_r(dev, CDREG_VERSION));

    dev->sysclk = cdctl_sys_cal(target_baud);
    pllcfg_t pll = cdctl_pll_cal(CDCTL_OSC_CLK, dev->sysclk);
    uint32_t actual_freq = cdctl_pll_get(CDCTL_OSC_CLK, pll);
    dn_info(dev->name, "sysclk %"PRIu32", actual: %"PRIu32"\n", dev->sysclk, actual_freq);
    dev->sysclk = actual_freq;
    cdctl_reg_w(dev, CDREG_PLL_N, pll.n);
    cdctl_reg_w(dev, CDREG_PLL_ML, pll.m & 0xff);
    cdctl_reg_w(dev, CDREG_PLL_OD_MH, (pll.d << 4) | (pll.m >> 8));
    dn_info(dev->name, "pll_n: %02x, ml: %02x, od_mh: %02x\n",
            cdctl_reg_r(dev, CDREG_PLL_N), cdctl_reg_r(dev, CDREG_PLL_ML), cdctl_reg_r(dev, CDREG_PLL_OD_MH));
    dn_info(dev->name, "pll_ctrl: %02x\n", cdctl_reg_r(dev, CDREG_PLL_CTRL));
    cdctl_reg_w(dev, CDREG_PLL_CTRL, 0x10); // enable pll
    dn_info(dev->name, "clk_status: %02x\n", cdctl_reg_r(dev, CDREG_CLK_STATUS));
    cdctl_reg_w(dev, CDREG_CLK_CTRL, 0x01); // select pll
    while (!(cdctl_reg_r(dev, CDREG_CLK_STATUS) & 0b100)) {}
    dn_info(dev->name, "clk_status (clk: pll): %02x\n", cdctl_reg_r(dev, CDREG_CLK_STATUS));
    dn_info(dev->name, "version (clk: pll): %02x\n", cdctl_reg_r(dev, CDREG_VERSION));
}


int cdctl_dev_init(cdctl_dev_t *dev, list_head_t *free_head, cdctl_cfg_t *init,
        spi_t *spi, gpio_t *int_n, irq_t int_irq)
{
    if (!dev->name)
        dev->name = "cdctl";
    dev->rx_frame = cd_list_get(free_head);
    dev->free_head = free_head;
    dev->cd_dev.get_rx_frame = cdctl_get_rx_frame;
    dev->cd_dev.put_tx_frame = cdctl_put_tx_frame;

#ifdef CD_USE_DYNAMIC_INIT
    dev->state = CDCTL_RST;
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->tx_wait_trigger = NULL;
    dev->tx_buf_clean_mask = false;
    dev->rx_cnt = 0;
    dev->tx_cnt = 0;
    dev->rx_lost_cnt = 0;
    dev->rx_error_cnt = 0;
    dev->rx_break_cnt = 0;
    dev->tx_cd_cnt = 0;
    dev->tx_error_cnt = 0;
    dev->rx_no_free_node_cnt = 0;
    dev->rx_len_err_cnt = 0;
#endif

    dev->spi = spi;
    dev->int_n = int_n;
    dev->int_irq = int_irq;

    dn_info(dev->name, "init...\n");
    uint8_t ver = cdctl_reg_r(dev, CDREG_VERSION);
    dn_info(dev->name, "chip version: %02x\n", ver);
    if (ver != 0x10) {
        dn_error(dev->name, "version error\n");
        return -1;
    }

    cdctl_reg_w(dev, CDREG_CLK_CTRL, 0x80); // soft reset
    cdctl_set_clk(dev, init->baud_h);
#ifndef CDCTL_AVOID_PIN_RE
    cdctl_reg_w(dev, CDREG_PIN_RE_CTRL, 0x10); // enable phy rx
#endif

    uint8_t setting = (cdctl_reg_r(dev, CDREG_SETTING) & 0xf) | CDBIT_SETTING_TX_PUSH_PULL;
    setting |= init->mode == 1 ? CDBIT_SETTING_BREAK_SYNC : CDBIT_SETTING_ARBITRATE;
    cdctl_reg_w(dev, CDREG_SETTING, setting);
    cdctl_reg_w(dev, CDREG_FILTER, init->mac);
    cdctl_reg_w(dev, CDREG_FILTER_M0, init->filter_m[0]);
    cdctl_reg_w(dev, CDREG_FILTER_M1, init->filter_m[1]);
    cdctl_reg_w(dev, CDREG_TX_PERMIT_LEN_L, init->tx_permit_len & 0xff);
    cdctl_reg_w(dev, CDREG_TX_PERMIT_LEN_H, init->tx_permit_len >> 8);
    cdctl_reg_w(dev, CDREG_MAX_IDLE_LEN_L, init->max_idle_len & 0xff);
    cdctl_reg_w(dev, CDREG_MAX_IDLE_LEN_H, init->max_idle_len >> 8);
    cdctl_reg_w(dev, CDREG_TX_PRE_LEN, init->tx_pre_len);
    cdctl_set_baud_rate(dev, init->baud_l, init->baud_h);
    cdctl_flush(dev);

    cdctl_get_baud_rate(dev, &init->baud_l, &init->baud_h);
    dn_debug(dev->name, "get baud rate: %"PRIu32" %"PRIu32"\n", init->baud_l, init->baud_h);
    dn_debug(dev->name, "get filter(m): %02x (%02x %02x)\n",
            cdctl_reg_r(dev, CDREG_FILTER), cdctl_reg_r(dev, CDREG_FILTER_M0), cdctl_reg_r(dev, CDREG_FILTER_M1));
    dn_debug(dev->name, "flags: %02x\n", cdctl_reg_r(dev, CDREG_INT_FLAG));
    dev->state = CDCTL_IDLE;
    cdctl_reg_w(dev, CDREG_INT_MASK, CDCTL_MASK);
    return 0;
}


static inline void cdctl_reg_r_it(cdctl_dev_t *dev, uint8_t reg)
{
    dev->buf[0] = reg;
    gpio_set_low(dev->spi->ns_pin);
    spi_wr_it(dev->spi, dev->buf, dev->buf, 2);
}

static inline void cdctl_reg_w_it(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
    dev->buf[0] = reg | 0x80;
    dev->buf[1] = val;
    gpio_set_low(dev->spi->ns_pin);
    spi_wr_it(dev->spi, dev->buf, NULL, 2);
}


// int_n pin interrupt isr
void cdctl_int_isr(cdctl_dev_t *dev)
{
    if (dev->state == CDCTL_IDLE || dev->state == CDCTL_WAIT_TX_CLEAN) {
        dev->state = CDCTL_RD_FLAG;
        cdctl_reg_r_it(dev, CDREG_INT_FLAG);
    }
}

// dma finish callback
void cdctl_spi_isr(cdctl_dev_t *dev)
{
    // end of CDCTL_RD_FLAG
    if (dev->state == CDCTL_RD_FLAG) {
        uint8_t val = dev->buf[1];
        gpio_set_high(dev->spi->ns_pin);

        if (val & (CDBIT_FLAG_RX_LOST | CDBIT_FLAG_RX_ERROR | CDBIT_FLAG_RX_BREAK | \
                CDBIT_FLAG_TX_CD | CDBIT_FLAG_TX_ERROR)) {
            if (val & CDBIT_FLAG_RX_LOST)
                dev->rx_lost_cnt++;
            if (val & CDBIT_FLAG_RX_ERROR)
                dev->rx_error_cnt++;
            if (val & CDBIT_FLAG_RX_BREAK)
                dev->rx_break_cnt++;
            if (val & CDBIT_FLAG_TX_CD)
                dev->tx_cd_cnt++;
            if (val & CDBIT_FLAG_TX_ERROR)
                dev->tx_error_cnt++;
        }

        // check for new frame
        if (val & CDBIT_FLAG_RX_PENDING) {
            dev->state = CDCTL_RX_HEADER;
            uint8_t *buf = dev->rx_frame->dat - 1;
            *buf = CDREG_RX; // borrow space from the "node" item
            gpio_set_low(dev->spi->ns_pin);
            spi_wr_it(dev->spi, buf, buf, 4);
            return;
        }

        // check for tx
        if (dev->tx_wait_trigger) {
            if (val & CDBIT_FLAG_TX_BUF_CLEAN) {
                dev->state = CDCTL_REG_W;
                cdctl_reg_w_it(dev, CDREG_TX_CTRL, CDBIT_TX_START | CDBIT_TX_RST_POINTER);
                cdctl_tx_cb(dev, dev->tx_wait_trigger);
                dev->tx_wait_trigger = NULL;
                return;
            } else if (!dev->tx_buf_clean_mask) {
                // enable tx_buf_clean irq
                dev->tx_buf_clean_mask = true;
                dev->state = CDCTL_REG_W;
                cdctl_reg_w_it(dev, CDREG_INT_MASK, CDCTL_MASK | CDBIT_FLAG_TX_BUF_CLEAN);
                return;
            }
        } else if (dev->tx_head.first) {
            dev->tx_frame = cd_list_get(&dev->tx_head);
            uint8_t *buf = dev->tx_frame->dat - 1;
            *buf = CDREG_TX | 0x80; // borrow space from the "node" item
            dev->state = CDCTL_TX_FRAME;
            gpio_set_low(dev->spi->ns_pin);
            spi_wr_it(dev->spi, buf, NULL, 4 + buf[3]);
            return;
        } else if (dev->tx_buf_clean_mask) {
            dev->tx_buf_clean_mask = false;
            dev->state = CDCTL_REG_W;
            cdctl_reg_w_it(dev, CDREG_INT_MASK, CDCTL_MASK);
            return;
        }

        dev->state = dev->tx_wait_trigger ? CDCTL_WAIT_TX_CLEAN : CDCTL_IDLE;
        if (!gpio_get_val(dev->int_n))
            cdctl_int_isr(dev);
        return;
    }

    // end of write RX_CTRL, TX_CTRL, INT_MASK
    if (dev->state == CDCTL_REG_W) {
        gpio_set_high(dev->spi->ns_pin);
        dev->state = CDCTL_RD_FLAG;
        cdctl_reg_r_it(dev, CDREG_INT_FLAG);
        return;
    }

    // end of CDCTL_RX_HEADER
    if (dev->state == CDCTL_RX_HEADER) {
        dev->state = CDCTL_RX_BODY;
        if (dev->rx_frame->dat[2] > min(CD_FRAME_SIZE - 3, 253)) {
            dev->rx_len_err_cnt++;
            dev->state = CDCTL_REG_W;
            cdctl_reg_w_it(dev, CDREG_RX_CTRL, CDBIT_RX_CLR_PENDING | CDBIT_RX_RST_POINTER);
            return;
        }
        if (dev->rx_frame->dat[2] != 0) {
            spi_wr_it(dev->spi, NULL, dev->rx_frame->dat + 3, dev->rx_frame->dat[2]);
            return;
        } // no return
    }

    // end of CDCTL_RX_BODY
    if (dev->state == CDCTL_RX_BODY) {
        gpio_set_high(dev->spi->ns_pin);
        dev->state = CDCTL_REG_W;
        cdctl_reg_w_it(dev, CDREG_RX_CTRL, CDBIT_RX_CLR_PENDING | CDBIT_RX_RST_POINTER);
        cd_frame_t *frame = cd_list_get(dev->free_head);
        if (frame) {
            cd_list_put(&dev->rx_head, dev->rx_frame);
            dev->rx_cnt++;
            cdctl_rx_cb(dev, dev->rx_frame);
            dev->rx_frame = frame;
        } else {
            dev->rx_no_free_node_cnt++;
        }
        return;
    }

    // end of CDCTL_TX_FRAME
    if (dev->state == CDCTL_TX_FRAME) {
        gpio_set_high(dev->spi->ns_pin);
#ifndef CDCTL_TX_NOT_FREE
        cd_list_put(dev->free_head, dev->tx_frame);
#endif
        dev->tx_wait_trigger = dev->tx_frame;
        dev->tx_frame = NULL;
        dev->tx_cnt++;

        dev->state = CDCTL_RD_FLAG;
        cdctl_reg_r_it(dev, CDREG_INT_FLAG);
        return;
    }

    dn_warn(dev->name, "unexpected spi dma cb\n");
}


__weak void cdctl_rx_cb(cdctl_dev_t *dev, cd_frame_t *frame) {}
__weak void cdctl_tx_cb(cdctl_dev_t *dev, cd_frame_t *frame) {}
