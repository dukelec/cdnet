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
#include "6lo_dispatcher.h"
#include "6lo_nd.h"
#include "main.h"


extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi1;

uart_t debug_uart = { .huart = &huart1 };

static gpio_t cdctl_ns = { .group = CDCTL_NS_GPIO_Port, .num = CDCTL_NS_Pin };
static spi_t cdctl_spi = { .hspi = &hspi1, .ns_pin = &cdctl_ns };

#define CD_FRAME_MAX 10
static cd_frame_t cd_frame_alloc[CD_FRAME_MAX];
static list_head_t cd_free_head = {0};

#define LO_PACKET_MAX 10
static lo_packet_t lo_packet_alloc[LO_PACKET_MAX];
static list_head_t lo_free_head = {0};

static cdctl_intf_t cdctl_intf = {0};
static lo_intf_t lo_intf = {0};
static lo_dispr_t lo_dispr = {0};
static lo_nd_t lo_nd = {0};
static udp_ser_t dev_info_ser = {0};


static void net_init(void)
{
    int i;
    for (i = 0; i < CD_FRAME_MAX; i++)
        list_put(&cd_free_head, &cd_frame_alloc[i].node);
    for (i = 0; i < LO_PACKET_MAX; i++)
        list_put(&lo_free_head, &lo_packet_alloc[i].node);

    cdctl_intf_init(&cdctl_intf, &cd_free_head, &cdctl_spi);
    lo_intf_init(&lo_intf, &lo_free_head, &cdctl_intf.cd_intf, 255);
    lo_dispatcher_init(&lo_dispr, &lo_intf);

    lo_nd_init(&lo_nd, &lo_intf, &lo_dispr);

    dev_info_ser.port = 1000; // listen on UDP port 1000
    list_put(&lo_dispr.udp_ser_head, &dev_info_ser.node);
}

static void dev_info_ser_cb(void)
{
    char info_str[100];

    list_node_t *node = list_get(&dev_info_ser.A_head);
    if (node) {
        lo_packet_t *pkt = container_of(node, lo_packet_t, node);

        // M: model; S: serial string; HW: hardware version; SW: software version
        sprintf(info_str, "M: uf_xxx; S: 5563c3ea3380; HW: v0.1; SW: v0.1");

        // filter string by input data
        if (pkt->dat_len != 0) {
            if (strstr(info_str, (char *)pkt->dat) == NULL) {
                list_put(lo_intf.free_head, &pkt->node);
                return;
            }
        }

        strcpy((char *)pkt->dat, info_str);
        pkt->dat_len = strlen(info_str);
        list_put(&lo_dispr.V_ser_head, node);
    }
}

static void init_rand(void)
{
    int i;
    for (i = 0; i < 50; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        srand(rand() + HAL_ADC_GetValue(&hadc1));
    }
}

void net_task(void)
{
    debug_init();
    net_init();
    init_rand();
    d_debug("rand(): %d\n", rand());

    while (true) {
        cdctl_task(&cdctl_intf);
        lo_dispatcher_task(&lo_dispr);

        nd_task(&lo_nd);
        dev_info_ser_cb();
        // app2 ...
        // ...

        debug_flush();
    }
}

