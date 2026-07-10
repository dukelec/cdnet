/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cd_utils.h"
#include "cd_debug.h"


static const char hex_tbl[] = "0123456789abcdef";

char *put_hex8(char *p, uint8_t val)
{
    *p++ = hex_tbl[val >> 4];
    *p++ = hex_tbl[val & 0xf];
    return p;
}

char *put_str(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}


// pbuf == NULL: print through d_puts directly
// pbuf size >= (limit * 3 - 1) + 4 + 1: hex with spaces + " ..." + '\0'

void hex_dump_small(char *pbuf, const void *addr, int len, int limit)
{
    int i;
    int dump_len = min(len, limit);
    const uint8_t *pc = (const uint8_t *)addr;
    char tbuf[pbuf ? 1 : limit * 3 + 4]; // " xx" per byte + " ..." + '\0'
    char *p = pbuf ? pbuf : tbuf;

    for (i = 0; i < dump_len; i++) {
        if (i)
            *p++ = ' ';
        p = put_hex8(p, pc[i]);
    }
    if (dump_len != len)
        p = put_str(p, " ...");
    *p = '\0';
    if (!pbuf)
        d_puts(tbuf);
}

void hex_dump(const void *addr, int len)
{
    int i;
    char pbuf[75];
    char sbuf[17];
    char *p = pbuf;
    const uint8_t *pc = (const uint8_t *)addr;

    if (len == 0 || len < 0)
        return;

    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) {
                // finish and print the previous line
                p = put_str(p, "  ");
                p = put_str(p, sbuf);
                p = put_str(p, "\n");
                *p = '\0';
                d_puts(pbuf);
                p = pbuf;
            }
            // offset
            p = put_str(p, "  ");
            p = put_hex8(p, (i >> 8) & 0xff);
            p = put_hex8(p, i & 0xff);
            p = put_str(p, " ");
        }
        *p++ = ' ';
        p = put_hex8(p, pc[i]);

        // printable ascii character
        if (pc[i] < 0x20 || pc[i] > 0x7e)
            sbuf[i % 16] = '.';
        else
            sbuf[i % 16] = pc[i];
        sbuf[(i % 16) + 1] = '\0';
    }

    // pad out last line
    while ((i % 16) != 0) {
        p = put_str(p, "   ");
        i++;
    }
    // print the final ascii field
    p = put_str(p, "  ");
    p = put_str(p, sbuf);
    p = put_str(p, "\n");
    *p = '\0';
    d_puts(pbuf);
}
