/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include "common.h"
#include "modbus_crc.h"
#include "cdbus_bytewise.h"

// member functions

static list_node_t *cdbw_get_free_node(cd_intf_t *cd_intf)
{
    uint32_t flags;
    list_node_t *node;
    cdbw_intf_t *cdbw_intf = container_of(cd_intf, cdbw_intf_t, cd_intf);
    local_irq_save(flags);
    node = list_get(cdbw_intf->free_head);
    local_irq_restore(flags);
    return node;
}

static list_node_t *cdbw_get_rx_node(cd_intf_t *cd_intf)
{
    uint32_t flags;
    list_node_t *node;
    cdbw_intf_t *cdbw_intf = container_of(cd_intf, cdbw_intf_t, cd_intf);
    local_irq_save(flags);
    node = list_get(&cdbw_intf->rx_head);
    local_irq_restore(flags);
    return node;
}

static void cdbw_put_free_node(cd_intf_t *cd_intf, list_node_t *node)
{
    uint32_t flags;
    cdbw_intf_t *cdbw_intf = container_of(cd_intf, cdbw_intf_t, cd_intf);
    local_irq_save(flags);
    list_put(cdbw_intf->free_head, node);
    local_irq_restore(flags);
}

static void cdbw_put_tx_node(cd_intf_t *cd_intf, list_node_t *node)
{
    uint32_t flags;
    cdbw_intf_t *cdbw_intf = container_of(cd_intf, cdbw_intf_t, cd_intf);
    local_irq_save(flags);
    list_put(&cdbw_intf->tx_head, node);
    local_irq_restore(flags);
}

static void cdbw_set_mac_filter(cd_intf_t *cd_intf, uint8_t filter)
{
    cdbw_intf_t *cdbw_intf = container_of(cd_intf, cdbw_intf_t, cd_intf);
    cdbw_intf->rx_filter = filter;
}

void cdbw_intf_init(cdbw_intf_t *intf, list_head_t *free_head,
    uart_t *uart, gpio_t *rx_pin)
{
    intf->free_head = free_head;
#ifdef USE_DYNAMIC_INIT
    intf->rx_head.first = NULL;
    intf->rx_head.last = NULL;
    intf->tx_head.first = NULL;
    intf->tx_head.last = NULL;
#endif
    intf->cd_intf.set_mac_filter = cdbw_set_mac_filter;
    intf->cd_intf.get_free_node = cdbw_get_free_node;
    intf->cd_intf.get_rx_node = cdbw_get_rx_node;
    intf->cd_intf.put_free_node = cdbw_put_free_node;
    intf->cd_intf.put_tx_node = cdbw_put_tx_node;

#ifdef USE_DYNAMIC_INIT
    intf->time_cnt = 0;
#endif
    intf->uart = uart;
    intf->rx_pin = rx_pin;

    intf->rx_state = RX_WAIT_IDLE;
    intf->rx_node = list_get(intf->free_head);
    intf->rx_filter = 255;

    intf->tx_state = TX_WAIT_DATA;
#ifdef USE_DYNAMIC_INIT
    intf->tx_node = NULL;
#endif
}

// handlers

void cdbw_rx_pin_irq_handler(cdbw_intf_t *intf)
{
    intf->time_cnt = 0;
}

void cdbw_timer_handler(cdbw_intf_t *intf)
{
    if (!gpio_get_value(intf->rx_pin))
        intf->time_cnt = 0;
    else if (intf->time_cnt < 9999)
        intf->time_cnt++;

    // tx

    if (intf->tx_state == TX_WAIT_DATA) {
        list_node_t *node = list_get(&intf->tx_head);
        if (node) {
            intf->tx_node = node;
            intf->tx_time_wait = CDBW_IDLE_CNT * 2 +
                    rand() / (RAND_MAX / CDBW_TX_CNT_RANGE);
            intf->tx_cd_cnt = 0;
            intf->tx_state = TX_KEEP_IDLE;
        }
    }

    if (intf->tx_state == TX_KEEP_IDLE) {
        if (intf->time_cnt > intf->tx_time_wait) {
            cd_frame_t *frame = container_of(intf->tx_node, cd_frame_t, node);
            intf->tx_byte_cnt = 0;
            intf->tx_crc = 0xffff;
            intf->tx_state = TX_BUSY;
            // disable rx_pin interrupt
            intf->time_cnt = 0; // force to zero
            d_verbose("cdbw %p: tx start\n", intf);
            uart_transmit(intf->uart, &frame->dat[0], 1);
        }
    }

    if (intf->tx_state == TX_BUSY) {
        if (intf->time_cnt > CDBW_IDLE_CNT) {
            d_verbose("cdbw %p: tx timeout\n", intf);
            intf->tx_state = TX_KEEP_IDLE;
        }
    }

    // rx

    if (intf->rx_state == RX_WAIT_IDLE) {
        if (intf->time_cnt > CDBW_IDLE_CNT) {
            cd_frame_t *frame = container_of(intf->rx_node, cd_frame_t, node);
            d_verbose("cdbw %p: rx init\n", intf);
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
            intf->rx_state = RX_WAIT_DATA;
            uart_receive_flush(intf->uart);
            uart_receive_it(intf->uart, &frame->dat[0], 1); // must ok
        }
    }

    if (intf->rx_state == RX_BUSY) {
        if (intf->time_cnt > CDBW_IDLE_CNT) {
            uart_abort_receive_it(intf->uart);
            intf->time_cnt = 0; // force to zero
            intf->rx_state = RX_WAIT_IDLE;
            d_debug("cdbw %p: rx timeout\n", intf);
        }
    }
}

void cdbw_rx_cplt_handler(cdbw_intf_t *intf)
{
    intf->time_cnt = 0;

    cd_frame_t *frame = container_of(intf->rx_node, cd_frame_t, node);
    intf->rx_state = RX_BUSY;

    //d_verbose("cdbw %p: rx %d: %02x\n", intf, intf->rx_byte_cnt,
    //    frame->dat[intf->rx_byte_cnt]);

    // for TX
    if (intf->rx_byte_cnt == 0 && intf->tx_state == TX_BUSY) {
        cd_frame_t *tx_frame = container_of(intf->tx_node, cd_frame_t, node);
        if (tx_frame->dat[0] == frame->dat[0]) {
            d_verbose("cdbw %p: continue tx\n", intf);
            cdbw_tx_cplt_handler(intf);
        } else {
            intf->tx_cd_cnt++;
            if (intf->tx_cd_cnt > 3) {
                list_put(intf->free_head, intf->tx_node);
                intf->tx_node = NULL;
                intf->tx_state = TX_WAIT_DATA;
                // set cd_error flag
                d_debug("cdbw %p: cd error\n", intf);
            } else {
                intf->tx_time_wait = CDBW_IDLE_CNT +
                        rand() / (RAND_MAX / CDBW_TX_CNT_RANGE);
                intf->tx_state = TX_KEEP_IDLE;
                d_debug("cdbw %p: cd++\n", intf);
            }
            // enable rx_pin interrupt
        }
    }

    // for RX

    intf->rx_crc = crc16_byte(frame->dat[intf->rx_byte_cnt], intf->rx_crc);

    if (intf->rx_byte_cnt == 1 && intf->rx_filter != 255) {
        if (frame->dat[0] == intf->rx_filter ||
                (frame->dat[1] != 255 && frame->dat[1] != intf->rx_filter)) {
            intf->rx_state = RX_WAIT_IDLE;
            return;
        }
    }
    if (intf->rx_byte_cnt >= 2 && intf->rx_byte_cnt == frame->dat[2] + 4) {

        if (intf->rx_crc != 0) {
            // set rx_error flag
            d_debug("cdbw %p: rx crc error\n", intf);
        } else {
            list_node_t *node = list_get(intf->free_head);
            if (node != NULL) {
                d_verbose("cdbw %p: rx done\n", intf);
                list_put(&intf->rx_head, intf->rx_node);
                intf->rx_node = node;
            } else {
                // set rx_lost flag
                d_debug("cdbw %p: rx lost\n", intf);
            }
        }
        intf->rx_state = RX_WAIT_IDLE;
        return;
    }

    intf->rx_byte_cnt++;
    uart_receive_it(intf->uart, &frame->dat[intf->rx_byte_cnt], 1);
}

void cdbw_tx_cplt_handler(cdbw_intf_t *intf)
{
    cd_frame_t *frame = container_of(intf->tx_node, cd_frame_t, node);

    intf->tx_crc = crc16_byte(frame->dat[intf->tx_byte_cnt], intf->tx_crc);

    //d_verbose("cdbw %p: tx %d, %02x\n", intf, intf->tx_byte_cnt,
    //    frame->dat[intf->tx_byte_cnt]);

    if (intf->tx_byte_cnt >= 2) {
        if (intf->tx_byte_cnt == frame->dat[2] + 2) {
            frame->dat[frame->dat[2] + 3] = intf->tx_crc & 0xff;
            frame->dat[frame->dat[2] + 4] = intf->tx_crc >> 8;
        }
        if (intf->tx_byte_cnt == frame->dat[2] + 4) {
            list_put(intf->free_head, intf->tx_node);
            intf->tx_node = NULL;
            intf->tx_state = TX_WAIT_DATA;
            // enable rx_pin interrupt
            d_verbose("cdbw %p: tx done\n", intf);
            return;
        }
    }

    intf->tx_byte_cnt++;
    uart_transmit_it(intf->uart, &frame->dat[intf->tx_byte_cnt], 1);
}

