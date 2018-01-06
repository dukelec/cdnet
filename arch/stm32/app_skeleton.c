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
#include "port_dispatcher.h"
#include "main.h"


extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi1;

uart_t debug_uart = { .huart = &huart1 };

static gpio_t cdctl_ns = { .group = CDCTL_NS_GPIO_Port, .num = CDCTL_NS_Pin };
static spi_t cdctl_spi = { .hspi = &hspi1, .ns_pin = &cdctl_ns };

#define CD_FRAME_MAX 10
static cd_frame_t cd_frame_alloc[CD_FRAME_MAX];
static list_head_t cd_free_head = {0};

#define NET_PACKET_MAX 10
static cdnet_packet_t net_packet_alloc[NET_PACKET_MAX];
static list_head_t net_free_head = {0};

static cdctl_intf_t cdctl_intf = {0};
static cdnet_intf_t net_intf = {0};
static port_dispr_t port_dispr = {0};
static udp_ser_t dev_info_ser = {0};


static void net_init(void)
{
    int i;
    for (i = 0; i < CD_FRAME_MAX; i++)
        list_put(&cd_free_head, &cd_frame_alloc[i].node);
    for (i = 0; i < NET_PACKET_MAX; i++)
        list_put(&net_free_head, &net_packet_alloc[i].node);

    cdctl_intf_init(&cdctl_intf, &cd_free_head, &cdctl_spi);
    cdnet_intf_init(&net_intf, &net_free_head, &cdctl_intf.cd_intf, 254);
    port_dispatcher_init(&port_dispr, &net_intf);

    dev_info_ser.port = 1; // listen on UDP port 1
    list_put(&port_dispr.udp_ser_head, &dev_info_ser.node);
}

static void dev_info_ser_cb(void)
{
    char info_str[100];

    list_node_t *node = list_get(&dev_info_ser.A_head);
    if (node) {
        cdnet_packet_t *pkt = container_of(node, cdnet_packet_t, node);

        // M: model; S: serial string; HW: hardware version; SW: software version
        sprintf(info_str, "M: uf_xxx; S: 5563c3ea3380; HW: v0.1; SW: v0.1");

        // filter string by input data
        if (pkt->dat_len != 0) {
            if (strstr(info_str, (char *)pkt->dat) == NULL) {
                list_put(net_intf.free_head, &pkt->node);
                return;
            }
        }

        strcpy((char *)pkt->dat, info_str);
        pkt->dat_len = strlen(info_str);
        list_put(&port_dispr.V_ser_head, node);
    }
}

void net_task(void)
{
    debug_init();
    net_init();

    while (true) {
        cdctl_task(&cdctl_intf);
        port_dispatcher_task(&port_dispr);

        dev_info_ser_cb();
        // app2 ...
        // ...

        debug_flush();
    }
}

