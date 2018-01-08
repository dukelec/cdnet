/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __PORT_DISPATCHER_H__
#define __PORT_DISPATCHER_H__

#include "common.h"
#include "cdnet.h"


typedef struct {
    list_node_t node;
    uint16_t    begin;  // e.g.: 2000
    uint16_t    end;    // e.g.: 2010
    uint16_t    cur;    // equal to begin at init
    list_head_t V_head; // V: up to down
    list_head_t A_head; // A: down to up
    bool        A_en;   // enable down to up
} udp_req_t;

typedef struct {
    list_node_t node;
    uint16_t    port;
    list_head_t A_head;
} udp_ser_t;


typedef struct {
    cdnet_intf_t *net_intf;

    list_head_t udp_req_head;
    list_head_t udp_ser_head;

    list_head_t V_ser_head;
} port_dispr_t;


static inline void port_dispatcher_init(port_dispr_t *dispr,
        cdnet_intf_t *intf)
{
    dispr->net_intf = intf;

#ifdef USE_DYNAMIC_INIT
    list_head_init(&dispr->udp_req_head);
    list_head_init(&dispr->udp_ser_head);
    list_head_init(&dispr->icmp_ser_head);
    list_head_init(&dispr->V_ser_head);
#endif
}

void port_dispatcher_task(port_dispr_t *dispr);

#endif

