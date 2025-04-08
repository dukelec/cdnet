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

typedef struct {
    list_node_t node;
//  uint8_t     _pad;
    uint8_t     dat[CD_FRAME_SIZE];
} cd_frame_t;

#ifdef CD_IRQ_SAFE
#define cd_list_get(head)               list_get_entry_it(head, cd_frame_t)
#define cd_list_put(head, frm)          list_put_it(head, &(frm)->node)
#elif !defined(CD_USER_LIST)
#define cd_list_get(head)               list_get_entry(head, cd_frame_t)
#define cd_list_put(head, frm)          list_put(head, &(frm)->node)
#endif

typedef struct cd_dev {
    cd_frame_t *(* get_rx_frame)(struct cd_dev *cd_dev);
    void (* put_tx_frame)(struct cd_dev *cd_dev, cd_frame_t *frame);
} cd_dev_t;

#endif
