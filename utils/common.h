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

// avoid something like: max(i++, j++), max(min(a, b), c) ...
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


#ifndef OVERRIDE_DEBUG
#ifndef dprintf
#define dprintf(fmt, ...) _dprintf(fmt, ## __VA_ARGS__)
#endif

#define d_info(fmt, ...) dprintf("I: " fmt, ## __VA_ARGS__)
#define d_warn(fmt, ...) dprintf("W: " fmt, ## __VA_ARGS__)
#define d_error(fmt, ...) dprintf("E: " fmt, ## __VA_ARGS__)

#ifdef VERBOSE
#define d_verbose(fmt, ...) dprintf("V: " fmt, ## __VA_ARGS__)
#ifndef DEBUG
#define DEBUG
#endif // DEBUG
#else
#define d_verbose(fmt, ...) do {} while (0)
#endif

#ifdef DEBUG
#define d_debug(fmt, ...) dprintf("D: " fmt, ## __VA_ARGS__)
#else
#define d_debug(fmt, ...) do {} while (0)
#endif

void _dprintf(char *format, ...);
void dputs(char *str);
void dhtoa(uint32_t val, char *buf);
void debug_init(void);
void debug_flush(void);
#endif // OVERRIDE_DEBUG

#endif

