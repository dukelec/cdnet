/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#include <unistd.h>
#include <time.h>
#include "common.h"
#include "arch_wrapper.h"


uint32_t get_systick(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int uart_transmit(uart_t *uart, const uint8_t *buf, uint16_t len)
{
    if (uart->is_stdout) {
        fwrite(buf, len, 1, stdout);
        return 0;
    }

    int ret = write(uart->fd, buf, len);
    return ret != len;
}

void _dprintf(char* format, ...)
{
    uint32_t flags;
    list_node_t *node;

    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}
