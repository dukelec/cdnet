/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDBUS_H__
#define __CDBUS_H__

#include "cd_utils.h"
#include "cd_list.h"

#ifndef CD_FRAME_SIZE
#define CD_FRAME_SIZE   260 // max size for cdbus through uart
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
