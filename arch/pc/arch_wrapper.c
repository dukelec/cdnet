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

void _dprintf(char* format, ...)
{
    uint32_t flags;
    list_node_t *node;

    va_list args;
    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}
