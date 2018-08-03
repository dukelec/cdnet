/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: http://www.modbustools.com/modbus_crc16.htm
 * Modified by: Duke Fong <duke@dukelec.com>
 */

#ifndef __MODBUS_CRC_H__
#define __MODBUS_CRC_H__

#include "cd_utils.h"

extern const uint16_t crc16_table[];

static inline void crc16_byte(uint8_t data, uint16_t *crc_val)
{
    uint8_t tmp;
    tmp = data ^ *crc_val;
    *crc_val >>= 8;
    *crc_val ^= crc16_table[tmp];
}

uint16_t crc16(const uint8_t *data, uint16_t length);

#endif

