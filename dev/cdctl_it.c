/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cdctl_it.h"

#define CDCTL_MASK (BIT_FLAG_RX_PENDING |           \
            BIT_FLAG_RX_LOST | BIT_FLAG_RX_ERROR |  \
            BIT_FLAG_TX_CD | BIT_FLAG_TX_ERROR)


// used by init and user configuration
uint8_t cdctl_read_reg(cdctl_dev_t *dev, uint8_t reg)
{
    uint8_t dat = 0xff;
    dev->manual_ctrl = true;
    while (dev->state > CDCTL_IDLE);
    spi_mem_read(dev->spi, reg, &dat, 1);
    dev->manual_ctrl = false;
    if (!gpio_get_value(dev->int_n)) {
        uint32_t flags;
        local_irq_save(flags);
        cdctl_int_isr(dev);
        local_irq_restore(flags);
    }
    return dat;
}

void cdctl_write_reg(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
    dev->manual_ctrl = true;
    while (dev->state > CDCTL_IDLE);
    spi_mem_write(dev->spi, reg | 0x80, &val, 1);
    dev->manual_ctrl = false;
    if (!gpio_get_value(dev->int_n)) {
        uint32_t flags;
        local_irq_save(flags);
        cdctl_int_isr(dev);
        local_irq_restore(flags);
    }
}


// member functions

cd_frame_t *cdctl_get_free_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return list_get_entry_it(dev->free_head, cd_frame_t);
}

cd_frame_t *cdctl_get_rx_frame(cd_dev_t *cd_dev)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    return list_get_entry_it(&dev->rx_head, cd_frame_t);
}

void cdctl_put_free_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    list_put_it(dev->free_head, &frame->node);
}

void cdctl_put_tx_frame(cd_dev_t *cd_dev, cd_frame_t *frame)
{
    uint32_t flags;
    cdctl_dev_t *dev = container_of(cd_dev, cdctl_dev_t, cd_dev);
    local_irq_save(flags);
    dev->tx_cnt++;
    list_put(&dev->tx_head, &frame->node);
    if (dev->state == CDCTL_IDLE)
        cdctl_int_isr(dev);
    local_irq_restore(flags);
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
        spi_t *spi, gpio_t *rst_n, gpio_t *int_n)
{
    if (!dev->name)
        dev->name = "cdctl";
    dev->rx_frame = list_get_entry(free_head, cd_frame_t);
    dev->free_head = free_head;
    dev->cd_dev.get_free_frame = cdctl_get_free_frame;
    dev->cd_dev.get_rx_frame = cdctl_get_rx_frame;
    dev->cd_dev.put_free_frame = cdctl_put_free_frame;
    dev->cd_dev.put_tx_frame = cdctl_put_tx_frame;

#ifdef USE_DYNAMIC_INIT
    dev->state = CDCTL_RST;
    dev->manual_ctrl = false;
    list_head_init(&dev->rx_head);
    list_head_init(&dev->tx_head);
    dev->tx_wait_trigger = false;
    dev->tx_buf_clean_mask = false;
    dev->rx_cnt = 0;
    dev->tx_cnt = 0;
    dev->rx_lost_cnt = 0;
    dev->rx_error_cnt = 0;
    dev->tx_cd_cnt = 0;
    dev->tx_error_cnt = 0;
    dev->rx_no_free_node_cnt = 0;
#endif

    dev->spi = spi;
    dev->rst_n = rst_n;
    dev->int_n = int_n;

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

    cdctl_write_reg(dev, REG_SETTING,
            cdctl_read_reg(dev, REG_SETTING) | BIT_SETTING_TX_PUSH_PULL);
    cdctl_write_reg(dev, REG_FILTER, filter);
    cdctl_set_baud_rate(dev, baud_l, baud_h);
    cdctl_flush(dev);

    dn_debug(dev->name, "flags: %02x\n", cdctl_read_reg(dev, REG_INT_FLAG));
    cdctl_write_reg(dev, REG_INT_MASK, CDCTL_MASK);
    dev->state = CDCTL_IDLE;
}


static inline
void cdctl_read_reg_it(cdctl_dev_t *dev, uint8_t reg)
{
    dev->buf[0] = reg;
    gpio_set_value(dev->spi->ns_pin, 0);
    spi_dma_write_read(dev->spi, dev->buf, dev->buf, 2);
}

static inline
void cdctl_write_reg_it(cdctl_dev_t *dev, uint8_t reg, uint8_t val)
{
    dev->buf[0] = reg | 0x80;
    dev->buf[1] = val;
    gpio_set_value(dev->spi->ns_pin, 0);
    spi_dma_write(dev->spi, dev->buf, 2);
}

// handlers

// int_n pin interrupt isr
void cdctl_int_isr(cdctl_dev_t *dev)
{
    if ((!dev->manual_ctrl && dev->state == CDCTL_IDLE) ||
            dev->state == CDCTL_WAIT_TX_CLEAN) {
        dev->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(dev, REG_INT_FLAG);
    }
}

// dma finish callback
void cdctl_spi_isr(cdctl_dev_t *dev)
{
    // end of CDCTL_RD_FLAG
    if (dev->state == CDCTL_RD_FLAG) {
        uint8_t val = dev->buf[1];
        uint8_t ret = 0;
        gpio_set_value(dev->spi->ns_pin, 1);

        // check rx error
        if (val & BIT_FLAG_RX_LOST) {
            ret |= BIT_RX_CLR_LOST;
            dev->rx_lost_cnt++;
        }
        if (val & BIT_FLAG_RX_ERROR) {
            ret |= BIT_RX_CLR_ERROR;
            dev->rx_error_cnt++;
        }
        if (val & BIT_FLAG_RX_BREAK) {
            ret |= BIT_RX_CLR_BREAK;
            dev->rx_break_cnt++;
        }
        if (ret) {
            dev->state = CDCTL_RX_CTRL;
            cdctl_write_reg_it(dev, REG_RX_CTRL, ret);
            return;
        }

        // check tx error
        if (val & BIT_FLAG_TX_CD) {
            ret |= BIT_TX_CLR_CD;
            dev->tx_cd_cnt++;
        }
        if (val & BIT_FLAG_TX_ERROR) {
            ret |= BIT_TX_CLR_ERROR;
            dev->tx_error_cnt++;
        }
        if (ret) {
            dev->state = CDCTL_TX_CTRL;
            cdctl_write_reg_it(dev, REG_TX_CTRL, ret);
            return;
        }

        // check for new frame
        if (val & BIT_FLAG_RX_PENDING) {
            dev->state = CDCTL_RX_HEADER;
            dev->buf[0] = REG_RX;
            gpio_set_value(dev->spi->ns_pin, 0);
            spi_dma_write_read(dev->spi, dev->buf, dev->buf, 4);
            return;
        }

        // check for tx
        if (dev->tx_wait_trigger) {
            if (val & BIT_FLAG_TX_BUF_CLEAN) {
                dev->tx_wait_trigger = false;

                dev->state = CDCTL_TX_CTRL;
                cdctl_write_reg_it(dev, REG_TX_CTRL,
                        BIT_TX_START | BIT_TX_RST_POINTER);
                return;
            } else if (!dev->tx_buf_clean_mask) {
                // enable tx_buf_clean irq
                dev->tx_buf_clean_mask = true;
                dev->state = CDCTL_TX_MASK;
                cdctl_write_reg_it(dev, REG_INT_MASK,
                        CDCTL_MASK | BIT_FLAG_TX_BUF_CLEAN);
                return;
            }
        } else if (dev->tx_head.first) {
            cd_frame_t *frame = list_entry(dev->tx_head.first, cd_frame_t);
            dev->buf[0] = REG_TX | 0x80;
            memcpy(dev->buf + 1, frame->dat, 3);
            dev->state = CDCTL_TX_HEADER;
            gpio_set_value(dev->spi->ns_pin, 0);
            spi_dma_write(dev->spi, dev->buf, 4);
            return;
        }

        if (dev->tx_buf_clean_mask) {
            dev->tx_buf_clean_mask = false;
            dev->state = CDCTL_TX_MASK;
            cdctl_write_reg_it(dev, REG_INT_MASK, CDCTL_MASK);
            return;
        }

        dev->state = dev->tx_wait_trigger ? CDCTL_WAIT_TX_CLEAN : CDCTL_IDLE;
        if (!gpio_get_value(dev->int_n))
            cdctl_int_isr(dev);
        return;
    }

    // end of CDCTL_RX_CTRL, TX_CTRL, TX_MASK
    if (dev->state == CDCTL_RX_CTRL ||
            dev->state == CDCTL_TX_CTRL ||
            dev->state == CDCTL_TX_MASK) {
        gpio_set_value(dev->spi->ns_pin, 1);
        dev->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(dev, REG_INT_FLAG);
        return;
    }

    // end of CDCTL_RX_HEADER
    if (dev->state == CDCTL_RX_HEADER) {
        memcpy(dev->rx_frame->dat, dev->buf + 1, 3);
        dev->state = CDCTL_RX_BODY;
        if (dev->rx_frame->dat[2] != 0) {
            spi_dma_read(dev->spi, dev->rx_frame->dat + 3,
                    dev->rx_frame->dat[2]);
            return;
        } // no return
    }

    // end of CDCTL_RX_BODY
    if (dev->state == CDCTL_RX_BODY) {
        gpio_set_value(dev->spi->ns_pin, 1);
        cd_frame_t *frame = list_get_entry_it(dev->free_head, cd_frame_t);
        if (frame) {
            list_put_it(&dev->rx_head, &dev->rx_frame->node);
            dev->rx_frame = frame;
            dev->rx_cnt++;
        } else {
            dev->rx_no_free_node_cnt++;
        }
        dev->state = CDCTL_RX_CTRL;
        cdctl_write_reg_it(dev, REG_RX_CTRL,
                BIT_RX_CLR_PENDING | BIT_RX_RST_POINTER);
        return;
    }

    // end of CDCTL_TX_HEADER
    if (dev->state == CDCTL_TX_HEADER) {
        cd_frame_t *frame = list_entry(dev->tx_head.first, cd_frame_t);
        dev->state = CDCTL_TX_BODY;
        if (frame->dat[2] != 0) {
            spi_dma_write(dev->spi, frame->dat + 3, frame->dat[2]);
            return;
        } // no return
    }

    // end of CDCTL_TX_BODY
    if (dev->state == CDCTL_TX_BODY) {
        gpio_set_value(dev->spi->ns_pin, 1);

        list_put_it(dev->free_head, list_get_it(&dev->tx_head));
        dev->tx_wait_trigger = true;

        dev->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(dev, REG_INT_FLAG);
        return;
    }

    dn_warn(dev->name, "unexpected spi dma cb\n");
}
