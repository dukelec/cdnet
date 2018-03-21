/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

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

#endif
