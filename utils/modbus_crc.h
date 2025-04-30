/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __MODBUS_CRC_H__
#define __MODBUS_CRC_H__

#include "cd_utils.h"

#ifdef CD_CRC_GEN_TBL
void crc16_table_init(void);
#endif

uint16_t crc16_sub(const uint8_t *data, uint32_t length, uint16_t crc_val);

static inline uint16_t crc16(const uint8_t *data, uint32_t length)
{
   return crc16_sub(data, length, 0xffff);
}

#endif
