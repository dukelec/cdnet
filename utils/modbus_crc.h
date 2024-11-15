/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: http://www.modbustools.com/modbus_crc16.htm
 * Modified by: Duke Fong <d@d-l.io>
 */

#ifndef __MODBUS_CRC_H__
#define __MODBUS_CRC_H__

#include "cd_utils.h"

void crc16_sub(const uint8_t *data, uint32_t length, uint16_t *crc_val);

static inline uint16_t crc16(const uint8_t *data, uint32_t length)
{
   uint16_t crc_val = 0xFFFF;
   crc16_sub(data, length, &crc_val);
   return crc_val;
}

#endif

