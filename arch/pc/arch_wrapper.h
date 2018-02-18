/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __ARCH_WRAPPER_H__
#define __ARCH_WRAPPER_H__

#include "common.h"


#define local_irq_save(flags)       \
    do { } while (0)
#define local_irq_restore(flags)    \
    do { } while (0)
#define local_irq_enable()          \
    do { } while (0)
#define local_irq_disable()         \
    do { } while (0)


typedef struct {
    char    *port;
    int     fd;
    bool    is_stdout;
} uart_t;

uint32_t get_systick(void);
int uart_transmit(uart_t *uart, const uint8_t *buf, uint16_t len);


#ifndef SYSTICK_US_DIV
#define SYSTICK_US_DIV  1000
#endif

#endif
