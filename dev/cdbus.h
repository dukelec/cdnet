/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDBUS_H__
#define __CDBUS_H__

#include "cd_utils.h"
#include "cd_list.h"

// 256 bytes are enough for the CDCTL controller (without CRC)
// 258 bytes are enough for the UART controller (with CRC)
// allow smaller sizes to save memory
#ifndef CD_FRAME_SIZE
#define CD_FRAME_SIZE   256
#endif

#ifndef CD_FRAME_TYPE
typedef struct {
    list_node_t node;
#ifdef CD_FRAME_PAD
    uint8_t     _pad; // align body (dat+3) to 32-bit, for dma (e.g. esp32xx)
#endif
    uint8_t     dat[CD_FRAME_SIZE];
} cd_frame_t;
#else
#include CD_FRAME_TYPE // user defined cd_frame_t, e.g. "cd_frame.h"
#endif

#ifdef CD_FRAME_PAD
_Static_assert((offsetof(cd_frame_t, dat) + 3) % 4 == 0, "dat+3 must be 32-bit aligned");
#endif

#ifdef CD_IRQ_SAFE
#define cd_list_get(head)               list_get_entry_it(head, cd_frame_t)
#define cd_list_get_last(head)          list_get_last_entry_it(head, cd_frame_t)
#define cd_list_put(head, frm)          list_put_it(head, &(frm)->node)
#elif !defined(CD_USER_LIST)
#define cd_list_get(head)               list_get_entry(head, cd_frame_t)
#define cd_list_get_last(head)          list_get_last_entry(head, cd_frame_t)
#define cd_list_put(head, frm)          list_put(head, &(frm)->node)
#endif

typedef struct cd_dev {
    cd_frame_t *(* recv_frame)(struct cd_dev *cd_dev);
    void (* send_frame)(struct cd_dev *cd_dev, cd_frame_t *frame);
} cd_dev_t;

#endif
