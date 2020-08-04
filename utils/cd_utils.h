/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CD_UTILS_H__
#define __CD_UTILS_H__

#include <errno.h>
#include <sys/unistd.h> // STDOUT_FILENO, STDERR_FILENO
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>     // provide offsetof, NULL
#include <stdint.h>

#ifdef __has_include
#if __has_include("cd_config.h")
#include "cd_config.h"
#endif
#else
#include "cd_config.h"
#endif

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
//#define NULL 0

#ifndef container_of
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

#if !__STRICT_ANSI__ && __GNUC__ >= 3

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

#ifndef __weak
#define __weak  __attribute__((weak))
#endif

#else

// simple version without GCC's statement expression ({...})
#ifndef sign
#define sign(a) (((a) < 0) ? -1 : ((a) > 0))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef clip
#define clip(a, b, c) ((a) < (b) ? (b) : ((a) > (c) ? (c) : (a)))
#endif
#define DIV_ROUND_CLOSEST(x, divisor) (((x) + ((divisor) / 2)) / (divisor))

#endif

#endif
