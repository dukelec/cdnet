/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "cdctl_it.h"

#define CDCTL_MASK (BIT_FLAG_RX_PENDING |           \
            BIT_FLAG_RX_LOST | BIT_FLAG_RX_ERROR |  \
            BIT_FLAG_TX_CD | BIT_FLAG_TX_ERROR)


// used by init and user configuration
uint8_t cdctl_read_reg(cdctl_intf_t *intf, uint8_t reg)
{
    uint8_t dat = 0xff;
    intf->manual_ctrl = true;
    while (intf->state > CDCTL_IDLE);
    spi_mem_read(intf->spi, reg, &dat, 1);
    intf->manual_ctrl = false;
    if (!gpio_get_value(intf->int_n)) {
        uint32_t flags;
        local_irq_save(flags);
        cdctl_int_isr(intf);
        local_irq_restore(flags);
    }
    return dat;
}
void cdctl_write_reg(cdctl_intf_t *intf, uint8_t reg, uint8_t val)
{
    intf->manual_ctrl = true;
    while (intf->state > CDCTL_IDLE);
    spi_mem_write(intf->spi, reg | 0x80, &val, 1);
    intf->manual_ctrl = false;
    if (!gpio_get_value(intf->int_n)) {
        uint32_t flags;
        local_irq_save(flags);
        cdctl_int_isr(intf);
        local_irq_restore(flags);
    }
}


// member functions

cd_frame_t *cdctl_get_free_frame(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get_entry_it(intf->free_head, cd_frame_t);
}

cd_frame_t *cdctl_get_rx_frame(cd_intf_t *cd_intf)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    return list_get_entry_it(&intf->rx_head, cd_frame_t);
}

void cdctl_put_free_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    list_put_it(intf->free_head, &frame->node);
}

void cdctl_put_tx_frame(cd_intf_t *cd_intf, cd_frame_t *frame)
{
    uint32_t flags;
    cdctl_intf_t *intf = container_of(cd_intf, cdctl_intf_t, cd_intf);
    local_irq_save(flags);
    intf->tx_cnt++;
    list_put(&intf->tx_head, &frame->node);
    if (intf->state == CDCTL_IDLE)
        cdctl_int_isr(intf);
    local_irq_restore(flags);
}


void cdctl_set_baud_rate(cdctl_intf_t *intf, uint32_t low, uint32_t high)
{
    uint16_t l, h;
    l = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, low) - 1;
    h = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, high) - 1;
    cdctl_write_reg(intf, REG_DIV_LS_L, l & 0xff);
    cdctl_write_reg(intf, REG_DIV_LS_H, l >> 8);
    cdctl_write_reg(intf, REG_DIV_HS_L, h & 0xff);
    cdctl_write_reg(intf, REG_DIV_HS_H, h >> 8);
    dn_debug(intf->name, "set baud rate: %u %u (%u %u)\n", low, high, l, h);
}

void cdctl_get_baud_rate(cdctl_intf_t *intf, uint32_t *low, uint32_t *high)
{
    uint16_t l, h;
    l = cdctl_read_reg(intf, REG_DIV_LS_L) |
            cdctl_read_reg(intf, REG_DIV_LS_H) << 8;
    h = cdctl_read_reg(intf, REG_DIV_HS_L) |
            cdctl_read_reg(intf, REG_DIV_HS_H) << 8;
    *low = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, l + 1);
    *high = DIV_ROUND_CLOSEST(CDCTL_SYS_CLK, h + 1);
}


void cdctl_intf_init(cdctl_intf_t *intf, list_head_t *free_head,
        uint8_t filter, uint32_t baud_l, uint32_t baud_h,
        spi_t *spi, gpio_t *rst_n, gpio_t *int_n)
{
    if (!intf->name)
        intf->name = "cdctl";
    intf->rx_frame = list_get_entry(free_head, cd_frame_t);
    intf->free_head = free_head;
    intf->cd_intf.get_free_frame = cdctl_get_free_frame;
    intf->cd_intf.get_rx_frame = cdctl_get_rx_frame;
    intf->cd_intf.put_free_frame = cdctl_put_free_frame;
    intf->cd_intf.put_tx_frame = cdctl_put_tx_frame;

#ifdef USE_DYNAMIC_INIT
    intf->state = CDCTL_RST;
    intf->manual_ctrl = false;
    list_head_init(&intf->rx_head);
    list_head_init(&intf->tx_head);
    intf->tx_wait_trigger = false;
    intf->tx_buf_clean_mask = false;
    intf->rx_cnt = 0;
    intf->tx_cnt = 0;
    intf->rx_lost_cnt = 0;
    intf->rx_error_cnt = 0;
    intf->tx_cd_cnt = 0;
    intf->tx_error_cnt = 0;
    intf->rx_no_free_node_cnt = 0;
#endif

    intf->spi = spi;
    intf->rst_n = rst_n;
    intf->int_n = int_n;

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
    cdctl_write_reg(intf, REG_FILTER, filter);
    cdctl_set_baud_rate(intf, baud_l, baud_h);
    cdctl_flush(intf);

    dn_debug(intf->name, "flags: %02x\n", cdctl_read_reg(intf, REG_INT_FLAG));
    cdctl_write_reg(intf, REG_INT_MASK, CDCTL_MASK);
    intf->state = CDCTL_IDLE;
    // enable int_n interrupt at outside
}


static inline
void cdctl_read_reg_it(cdctl_intf_t *intf, uint8_t reg)
{
    intf->buf[0] = reg;
    gpio_set_value(intf->spi->ns_pin, 0);
    spi_dma_write_read(intf->spi, intf->buf, intf->buf, 2);
}

static inline
void cdctl_write_reg_it(cdctl_intf_t *intf, uint8_t reg, uint8_t val)
{
    intf->buf[0] = reg | 0x80;
    intf->buf[1] = val;
    gpio_set_value(intf->spi->ns_pin, 0);
    spi_dma_write(intf->spi, intf->buf, 2);
}

// handlers

// int_n pin interrupt isr
void cdctl_int_isr(cdctl_intf_t *intf)
{
    if ((!intf->manual_ctrl && intf->state == CDCTL_IDLE) ||
            intf->state == CDCTL_WAIT_TX_CLEAN) {
        intf->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(intf, REG_INT_FLAG);
    }
}

// dma finish callback
void cdctl_spi_isr(cdctl_intf_t *intf)
{
    // end of CDCTL_RD_FLAG
    if (intf->state == CDCTL_RD_FLAG) {
        uint8_t val = intf->buf[1];
        uint8_t ret = 0;
        gpio_set_value(intf->spi->ns_pin, 1);

        // check rx error
        if (val & BIT_FLAG_RX_LOST) {
            ret |= BIT_RX_CLR_LOST;
            intf->rx_lost_cnt++;
        }
        if (val & BIT_FLAG_RX_ERROR) {
            ret |= BIT_RX_CLR_ERROR;
            intf->rx_error_cnt++;
        }
        if (ret) {
            intf->state = CDCTL_RX_CTRL;
            cdctl_write_reg_it(intf, REG_RX_CTRL, ret);
            return;
        }

        // check tx error
        if (val & BIT_FLAG_TX_CD) {
            ret |= BIT_TX_CLR_CD;
            intf->tx_cd_cnt++;
        }
        if (val & BIT_FLAG_TX_ERROR) {
            ret |= BIT_TX_CLR_ERROR;
            intf->tx_error_cnt++;
        }
        if (ret) {
            intf->state = CDCTL_TX_CTRL;
            cdctl_write_reg_it(intf, REG_TX_CTRL, ret);
            return;
        }

        // check for new frame
        if (val & BIT_FLAG_RX_PENDING) {
            intf->state = CDCTL_RX_HEADER;
            intf->buf[0] = REG_RX;
            gpio_set_value(intf->spi->ns_pin, 0);
            spi_dma_write_read(intf->spi, intf->buf, intf->buf, 4);
            return;
        }

        // check for tx
        if (intf->tx_wait_trigger) {
            if (val & BIT_FLAG_TX_BUF_CLEAN) {
                intf->tx_wait_trigger = false;

                intf->state = CDCTL_TX_CTRL;
                cdctl_write_reg_it(intf, REG_TX_CTRL, BIT_TX_START);
                return;
            } else if (!intf->tx_buf_clean_mask) {
                // enable tx_buf_clean irq
                intf->tx_buf_clean_mask = true;
                intf->state = CDCTL_TX_MASK;
                cdctl_write_reg_it(intf, REG_INT_MASK,
                        CDCTL_MASK | BIT_FLAG_TX_BUF_CLEAN);
                return;
            }
        } else if (intf->tx_head.first) {
            cd_frame_t *frame = list_entry(intf->tx_head.first, cd_frame_t);
            intf->buf[0] = REG_TX | 0x80;
            memcpy(intf->buf + 1, frame->dat, 3);
            intf->state = CDCTL_TX_HEADER;
            gpio_set_value(intf->spi->ns_pin, 0);
            spi_dma_write(intf->spi, intf->buf, 4);
            return;
        }

        if (intf->tx_buf_clean_mask) {
            intf->tx_buf_clean_mask = false;
            intf->state = CDCTL_TX_MASK;
            cdctl_write_reg_it(intf, REG_INT_MASK, CDCTL_MASK);
            return;
        }

        intf->state = intf->tx_wait_trigger ? CDCTL_WAIT_TX_CLEAN : CDCTL_IDLE;
        if (!gpio_get_value(intf->int_n))
            cdctl_int_isr(intf);
        return;
    }

    // end of CDCTL_RX_CTRL, TX_CTRL, TX_MASK
    if (intf->state == CDCTL_RX_CTRL ||
            intf->state == CDCTL_TX_CTRL ||
            intf->state == CDCTL_TX_MASK) {
        gpio_set_value(intf->spi->ns_pin, 1);
        intf->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(intf, REG_INT_FLAG);
        return;
    }

    // end of CDCTL_RX_HEADER
    if (intf->state == CDCTL_RX_HEADER) {
        memcpy(intf->rx_frame->dat, intf->buf + 1, 3);
        intf->state = CDCTL_RX_BODY;
        if (intf->rx_frame->dat[2] != 0) {
            spi_dma_read(intf->spi, intf->rx_frame->dat + 3,
                    intf->rx_frame->dat[2]);
            return;
        } // else goto next if block directly
    }

    // end of CDCTL_RX_BODY
    if (intf->state == CDCTL_RX_BODY) {
        gpio_set_value(intf->spi->ns_pin, 1);
        cd_frame_t *frame = list_get_entry_it(intf->free_head, cd_frame_t);
        if (frame) {
            list_put_it(&intf->rx_head, &intf->rx_frame->node);
            intf->rx_frame = frame;
            intf->rx_cnt++;
        } else {
            intf->rx_no_free_node_cnt++;
        }
        intf->state = CDCTL_RX_CTRL;
        cdctl_write_reg_it(intf, REG_RX_CTRL, BIT_RX_CLR_PENDING);
        return;
    }

    // end of CDCTL_TX_HEADER
    if (intf->state == CDCTL_TX_HEADER) {
        cd_frame_t *frame = list_entry(intf->tx_head.first, cd_frame_t);
        intf->state = CDCTL_TX_BODY;
        if (frame->dat[2] != 0) {
            spi_dma_write(intf->spi, frame->dat + 3, frame->dat[2]);
            return;
        } // else goto next if block directly
    }

    // end of CDCTL_TX_BODY
    if (intf->state == CDCTL_TX_BODY) {
        gpio_set_value(intf->spi->ns_pin, 1);

        list_put_it(intf->free_head, list_get_it(&intf->tx_head));
        intf->tx_wait_trigger = true;

        intf->state = CDCTL_RD_FLAG;
        cdctl_read_reg_it(intf, REG_INT_FLAG);
        return;
    }

    dn_warn(intf->name, "unexpected spi dma cb\n");
}
