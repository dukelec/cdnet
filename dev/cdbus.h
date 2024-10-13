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
    uint8_t     dat[CD_FRAME_SIZE];
} cd_frame_t;

typedef struct cd_dev {
    cd_frame_t *(* get_free_frame)(struct cd_dev *cd_dev);
    cd_frame_t *(* get_rx_frame)(struct cd_dev *cd_dev);
    void (* put_free_frame)(struct cd_dev *cd_dev, cd_frame_t *frame);
    void (* put_tx_frame)(struct cd_dev *cd_dev, cd_frame_t *frame);
} cd_dev_t;

#endif
