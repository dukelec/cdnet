/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>     // provide offsetof, NULL
#include <stdint.h>

#if __has_include("cdnet_config.h")
#include "cdnet_config.h"
#endif

#include "arch_wrapper.h"
#include "list.h"

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
//#define NULL 0

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#ifndef sign
#define sign(a) ({                  \
        typeof(a) __a = (a);        \
        __a < 0 ? -1 : (__a > 0);   \
    })
#endif
#ifndef max
#define max(a, b) ({                \
        typeof(a) __a = (a);        \
        typeof(b) __b = (b);        \
        __a > __b ? __a : __b;      \
    })
#endif
#ifndef min
#define min(a, b) ({                \
        typeof(a) __a = (a);        \
        typeof(b) __b = (b);        \
        __a < __b ? __a : __b;      \
    })
#endif
#ifndef clip
#define clip(a, b, c) ({                            \
        typeof(a) __a = (a);                        \
        typeof(b) __b = (b);                        \
        typeof(c) __c = (c);                        \
        __a < __b ? __b : (__a > __c ? __c : __a);  \
    })
#endif
#ifndef swap
#define swap(a, b) \
    do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)
#endif

/*
 * Divide positive or negative dividend by positive divisor and round
 * to closest integer. Result is undefined for negative divisors and
 * for negative dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)({                                     \
        typeof(x) __x = x;                                                  \
        typeof(divisor) __d = divisor;                                      \
        (((typeof(x))-1) > 0 || ((typeof(divisor))-1) > 0 || (__x) > 0) ?   \
                (((__x) + ((__d) / 2)) / (__d)) :                           \
                (((__x) - ((__d) / 2)) / (__d));                            \
    })


#ifndef OVERRIDE_DEBUG
#ifndef dprintf
#define dprintf(fmt, ...) _dprintf(fmt, ## __VA_ARGS__)
#endif

#define d_info(fmt, ...) dprintf("I: " fmt, ## __VA_ARGS__)
#define d_warn(fmt, ...) dprintf("W: " fmt, ## __VA_ARGS__)
#define d_error(fmt, ...) dprintf("E: " fmt, ## __VA_ARGS__)

#define dd_info(name, fmt, ...) dprintf("I: %s: " fmt, name, ## __VA_ARGS__)
#define dd_warn(name, fmt, ...) dprintf("W: %s: " fmt, name, ## __VA_ARGS__)
#define dd_error(name, fmt, ...) dprintf("E: %s: " fmt, name, ## __VA_ARGS__)

#ifdef VERBOSE
#define d_verbose(fmt, ...) dprintf("V: " fmt, ## __VA_ARGS__)
#define dd_verbose(name, fmt, ...) dprintf("V: %s: " fmt, name, ## __VA_ARGS__)
#ifndef DEBUG
#define DEBUG
#endif // DEBUG
#else
#define d_verbose(fmt, ...) do {} while (0)
#define dd_verbose(name, ...) do {} while (0)
#endif

#ifdef DEBUG
#define d_debug(fmt, ...) dprintf("D: " fmt, ## __VA_ARGS__)
#define dd_debug(name, fmt, ...) dprintf("D: %s: " fmt, name, ## __VA_ARGS__)
#else
#define d_debug(fmt, ...) do {} while (0)
#define dd_debug(name, ...) do {} while (0)
#endif

void _dprintf(char *format, ...);
void dputs(char *str);
void dhtoa(uint32_t val, char *buf);
void debug_init(void);
void debug_flush(void);

void hex_dump_small(char *pbuf, const void *addr, int len, int max);
void hex_dump(const void *addr, int len);
#endif // OVERRIDE_DEBUG

#endif

