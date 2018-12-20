/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __CDCTL_REGS_V0X_H__
#define __CDCTL_REGS_V0X_H__

#ifndef CDCTL_SYS_CLK
#define CDCTL_SYS_CLK       40000000UL // 40MHz for CDCTL-Bx
#endif

#define REG_VERSION         0x00
#define REG_SETTING         0x01
#define REG_IDLE_WAIT_LEN   0x02
#define REG_TX_WAIT_LEN     0x03
#define REG_FILTER          0x04
#define REG_DIV_LS_L        0x05
#define REG_DIV_LS_H        0x06
#define REG_DIV_HS_L        0x07
#define REG_DIV_HS_H        0x08
#define REG_INT_FLAG        0x09
#define REG_INT_MASK        0x0a
#define REG_RX              0x0b
#define REG_TX              0x0c
#define REG_RX_CTRL         0x0d
#define REG_TX_CTRL         0x0e
#define REG_RX_ADDR         0x0f
#define REG_RX_PAGE_FLAG    0x10
#define REG_FILTER1         0x11    // multicast filter
#define REG_FILTER2         0x12


#define BIT_SETTING_TX_PUSH_PULL    (1 << 0)
#define BIT_SETTING_TX_INVERT       (1 << 1)
#define BIT_SETTING_USER_CRC        (1 << 2)
#define BIT_SETTING_NO_DROP         (1 << 3)
#define POS_SETTING_TX_EN_DELAY           4
#define BIT_SETTING_NO_ARBITRATE    (1 << 6)
#define BIT_SETTING_FULL_DUPLEX     (1 << 7)

#define BIT_FLAG_BUS_IDLE           (1 << 0)
#define BIT_FLAG_RX_PENDING         (1 << 1)
#define BIT_FLAG_RX_LOST            (1 << 2)
#define BIT_FLAG_RX_ERROR           (1 << 3)
#define BIT_FLAG_TX_BUF_CLEAN       (1 << 4)
#define BIT_FLAG_TX_CD              (1 << 5)
#define BIT_FLAG_TX_ERROR           (1 << 6)

#define BIT_RX_RST_POINTER          (1 << 0)
#define BIT_RX_CLR_PENDING          (1 << 1)
#define BIT_RX_CLR_LOST             (1 << 2)
#define BIT_RX_CLR_ERROR            (1 << 3)
#define BIT_RX_RST                  (1 << 4)
#define BIT_RX_RST_ALL              0x1f

#define BIT_TX_RST_POINTER          (1 << 0)
#define BIT_TX_START                (1 << 1)
#define BIT_TX_CLR_CD               (1 << 2)
#define BIT_TX_CLR_ERROR            (1 << 3)
#define BIT_TX_ABORT                (1 << 4)

#endif
