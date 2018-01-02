/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: http://www.modbustools.com/modbus_crc16.htm
 * Modified by: Duke Fong <duke@dukelec.com>
 */

#ifndef __MODBUS_CRC_H__
#define __MODBUS_CRC_H__

#include "common.h"

uint16_t crc16_byte(uint8_t data, uint16_t crc_val);
uint16_t crc16(const uint8_t *data, uint16_t length);

#endif

