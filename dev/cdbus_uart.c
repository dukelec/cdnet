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
#include "cdbus_uart.h"

#ifdef CDUART_IT
#define cduart_irq_save(flags)    local_irq_save(flags)
#define cduart_irq_restore(flags) local_irq_restore(flags)
#else
#define cduart_irq_save(flags)    do {} while (0)
#define cduart_irq_restore(flags) do {} while (0)
#endif

// member functions

static list_node_t *cduart_get_free_node(cd_intf_t *cd_intf)
{
#ifdef CDUART_IT
    uint32_t flags;
#endif
    list_node_t *node;
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_irq_save(flags);
    node = list_get(intf->free_head);
    cduart_irq_restore(flags);
    return node;
}

static list_node_t *cduart_get_rx_node(cd_intf_t *cd_intf)
{
#ifdef CDUART_IT
    uint32_t flags;
#endif
    list_node_t *node;
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_irq_save(flags);
    node = list_get(&intf->rx_head);
    cduart_irq_restore(flags);
    return node;
}

static void cduart_put_free_node(cd_intf_t *cd_intf, list_node_t *node)
{
#ifdef CDUART_IT
    uint32_t flags;
#endif
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_irq_save(flags);
    list_put(intf->free_head, node);
    cduart_irq_restore(flags);
}

static void cduart_put_tx_node(cd_intf_t *cd_intf, list_node_t *node)
{
#ifdef CDUART_IT
    uint32_t flags;
#endif
    cduart_intf_t *intf = container_of(cd_intf, cduart_intf_t, cd_intf);
    cduart_irq_save(flags);
    list_put(&intf->tx_head, node);
    cduart_irq_restore(flags);
}


void cduart_intf_init(cduart_intf_t *intf, list_head_t *free_head,
    uart_t *uart)
{
    intf->free_head = free_head;

    intf->cd_intf.get_free_node = cduart_get_free_node;
    intf->cd_intf.get_rx_node = cduart_get_rx_node;
    intf->cd_intf.put_free_node = cduart_put_free_node;
    intf->cd_intf.put_tx_node = cduart_put_tx_node;

    intf->rx_node = list_get(free_head);

    intf->uart = uart;
    intf->t_last = get_systick();
    intf->rx_crc = 0xffff;

#ifdef USE_DYNAMIC_INIT
    intf->tx_head.first = NULL;
    intf->tx_head.last = NULL;
    intf->rx_head.first = NULL;
    intf->rx_head.last = NULL;

    intf->tx_node = NULL;
    intf->rx_byte_cnt = 0;

    // filters should set by caller
    intf->cd_intf.set_filter = NULL;
    intf->remote_filter_len = 0;
    intf->local_filter_len = 0;
#endif
}

// handler

static bool match_filter(uint8_t *filter, uint8_t filter_len, uint8_t val)
{
    uint8_t i;
    bool is_match = false;

    for (i = 0; i < filter_len; i++) {
        if (val == filter[i]) {
            is_match = true;
            break;
        }
    }
    return is_match;
}

void cduart_rx_task(cduart_intf_t *intf, uint8_t val)
{
    cd_frame_t *frame = container_of(intf->rx_node, cd_frame_t, node);
    //d_verbose("cduart: rx cnt: %d, val: %02x\n", intf->rx_byte_cnt, val);

    if (intf->rx_byte_cnt != 0 &&
            get_systick() - intf->t_last > CDUART_IDLE_TIME) {
        d_debug("cduart: drop packet, cnt: %d\n", intf->rx_byte_cnt);
        intf->rx_byte_cnt = 0;
        intf->rx_crc = 0xffff;
    }

    frame->dat[intf->rx_byte_cnt] = val;
    intf->rx_crc = crc16_byte(val, intf->rx_crc);

    if (intf->rx_byte_cnt == 0) {
        if (!match_filter(intf->remote_filter, intf->remote_filter_len, val)) {
            d_verbose("cduart: byte0 filtered, %02x\n", val);
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
            return;
        }
    } else if (intf->rx_byte_cnt == 1) {
        if (!match_filter(intf->local_filter, intf->local_filter_len, val)) {
            d_verbose("cduart: byte1 filtered, %02x\n", val);
            intf->rx_byte_cnt = 0;
            intf->rx_crc = 0xffff;
            return;
        }
    } else if (intf->rx_byte_cnt == frame->dat[2] + 4) {

        if (intf->rx_crc != 0) {
            d_debug("cduart: crc error\n");
        } else {
            list_node_t *node = list_get(intf->free_head);
            if (node != NULL) {
                list_put(&intf->rx_head, intf->rx_node);
                intf->rx_node = node;
            } else {
                // set rx_lost flag
                d_debug("cduart: rx_lost\n");
            }
        }
        intf->rx_byte_cnt = 0;
        intf->rx_crc = 0xffff;
        return;
    }
    intf->rx_byte_cnt++;
    intf->t_last = get_systick();
}

void cduart_tx_task(cduart_intf_t *intf)
{
#ifdef CDUART_TX_IT
    if (!uart_transmit_is_ready(intf->uart))
        return;
#endif

    list_node_t *node = list_get(&intf->tx_head);
    if (!node)
        return;
    cd_frame_t *frame = container_of(node, cd_frame_t, node);

    uint16_t crc_val = crc16(frame->dat, frame->dat[2] + 3);
    frame->dat[frame->dat[2] + 3] = crc_val & 0xff;
    frame->dat[frame->dat[2] + 4] = crc_val >> 8;

#ifdef CDUART_TX_IT
    uart_transmit_it(intf->uart, frame->dat, frame->dat[2] + 5);
#else
    uart_transmit(intf->uart, frame->dat, frame->dat[2] + 5);
#endif
    list_put(intf->free_head, node);
}

void cduart_task(cduart_intf_t *intf)
{
    uint8_t dat;
    int ret;

    while (true) {
        ret = uart_receive(intf->uart, &dat, 1);
        if (ret != 0)
            break;
        cduart_rx_task(intf, dat);
    }

    cduart_tx_task(intf);
}

