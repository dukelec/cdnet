/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CD_DEBUG_H__
#define __CD_DEBUG_H__

#include "cdnet_dispatch.h"

#ifndef d_printf
#define d_printf(fmt, ...)          _dprintf(fmt, ## __VA_ARGS__)
#endif
#ifndef d_puts
#define d_puts(str)                 _dputs(str)
#endif

#define d_info(fmt, ...)            d_printf("I: " fmt, ## __VA_ARGS__)
#ifndef d_warn
#define d_warn(fmt, ...)            d_printf("W: " fmt, ## __VA_ARGS__)
#endif
#ifndef d_error
#define d_error(fmt, ...)           d_printf("E: " fmt, ## __VA_ARGS__)
#endif

#define dn_info(name, fmt, ...)     d_info("%s: " fmt, name, ## __VA_ARGS__)
#define dn_warn(name, fmt, ...)     d_warn("%s: " fmt, name, ## __VA_ARGS__)
#define dn_error(name, fmt, ...)    d_error("%s: " fmt, name, ## __VA_ARGS__)

#define df_info(fmt, ...)           dn_info(__FUNCTION__, fmt, ## __VA_ARGS__)
#define df_warn(fmt, ...)           dn_warn(__FUNCTION__, fmt, ## __VA_ARGS__)
#define df_error(fmt, ...)          dn_error(__FUNCTION__, fmt, ## __VA_ARGS__)

#define dnf_info(name, fmt, ...)    d_info("%s: %s: " fmt, name, __FUNCTION__, ## __VA_ARGS__)
#define dnf_warn(name, fmt, ...)    d_warn("%s: %s: " fmt, name, __FUNCTION__, ## __VA_ARGS__)
#define dnf_error(name, fmt, ...)   d_error("%s: %s: " fmt, name, __FUNCTION__, ## __VA_ARGS__)

#ifdef VERBOSE
#define d_verbose_c(fmt, ...)       d_printf(fmt, ## __VA_ARGS__)
#define d_verbose(fmt, ...)         d_printf("V: " fmt, ## __VA_ARGS__)
#define dn_verbose(name, fmt, ...)  d_verbose("%s: " fmt, name, ## __VA_ARGS__)
#define df_verbose(fmt, ...)        dn_verbose(__FUNCTION__, fmt, ## __VA_ARGS__)
#define dnf_verbose(name, fmt, ...) d_verbose("%s: %s: " fmt, name, __FUNCTION__, ## __VA_ARGS__)
#ifndef DEBUG
#define DEBUG
#endif // DEBUG
#else
#define d_verbose_c(fmt, ...)       do {} while (0)
#define d_verbose(fmt, ...)         do {} while (0)
#define dn_verbose(name, ...)       do {} while (0)
#define df_verbose(name, ...)       do {} while (0)
#define dnf_verbose(name, ...)      do {} while (0)
#endif

#ifdef DEBUG
#define d_debug_c(fmt, ...)         d_printf(fmt, ## __VA_ARGS__)
#define d_debug(fmt, ...)           d_printf("D: " fmt, ## __VA_ARGS__)
#define dn_debug(name, fmt, ...)    d_debug("%s: " fmt, name, ## __VA_ARGS__)
#define df_debug(fmt, ...)          dn_debug(__FUNCTION__, fmt, ## __VA_ARGS__)
#define dnf_debug(name, fmt, ...)   d_debug("%s: %s: " fmt, name, __FUNCTION__, ## __VA_ARGS__)
#else
#define d_debug_c(fmt, ...)         do {} while (0)
#define d_debug(fmt, ...)           do {} while (0)
#define dn_debug(name, ...)         do {} while (0)
#define df_debug(name, ...)         do {} while (0)
#define dnf_debug(name, ...)        do {} while (0)
#endif

void _dprintf(char *format, ...);
void _dputs(char *str);
void dhtoa(uint32_t val, char *buf);
void debug_uart_init(void);
void debug_init(cdn_ns_t *ns, cdn_sockaddr_t *dst, bool *en);
void debug_flush(void);

void hex_dump_small(char *pbuf, const void *addr, int len, int max);
void hex_dump(const void *addr, int len);

#endif
