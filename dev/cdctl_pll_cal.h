/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDCTL_PLL_CAL_H__
#define __CDCTL_PLL_CAL_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t     n;
    uint16_t    m;
    uint8_t     d;
    uint32_t    freq;   // actual output frequency
} pllcfg_t;

pllcfg_t cdctl_pll_cal(uint32_t input, uint32_t output);

uint32_t cdctl_sys_cal(uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif
