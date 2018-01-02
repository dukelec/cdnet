/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __6LO_ND_H__
#define __6LO_ND_H__

#include "common.h"
#include "6lo_dispatcher.h"


#define ND_TYPE_NS          135
#define ND_TYPE_NA          136

#ifndef ND_NS_TIMEOUT
#define ND_NS_TIMEOUT       100 // ms
#endif
#ifndef ND_NA_TIMEOUT
#define ND_NA_TIMEOUT       500 // ms
#endif

#ifndef ND_INTERVAL
#define ND_INTERVAL         30000
#endif
#ifndef ND_TIMES
#define ND_TIMES            -1  // -1: forever, 0: never
#endif


typedef enum {
    ND_BEGIN = 0,
    ND_SEND_NS,
    ND_WAIT_RX,
    ND_IDLE
} nd_state_t;

typedef struct {
    nd_state_t  state;
    uint8_t     target_addr;
    uint32_t    t_last;    // ms
    uint32_t    identify;  // ND_OPTION_IDENTIFY
    int8_t      times;

    bool        if_get_self;
    bool        if_get_others;

    // TODO: choose random address by using addr_pool:
    // (promiscuous mode only)
    // when receive NA and NS, set addr_pool[src_mac] = 255
    // each one -- if > 0 for every interval_time
    // choose addr:
    //   N = how_many_zero_in_pool()
    //   R = random_range(0, N)
    //   return R'th zero item's index
    //uint8_t   addr_pool[255];

    lo_intf_t   *lo_intf;
    icmp_ser_t  ns_ser;
    icmp_ser_t  na_ser;
} lo_nd_t;


void lo_nd_init(lo_nd_t *intf, lo_intf_t *lo_intf, lo_dispr_t *dispr);
void nd_task(lo_nd_t *lo_nd);

#endif

