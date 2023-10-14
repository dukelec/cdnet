/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __CDCTL_REGS_H__
#define __CDCTL_REGS_H__

#define REG_VERSION         0x00
#define REG_SETTING         0x02
#define REG_IDLE_WAIT_LEN   0x04
#define REG_TX_PERMIT_LEN_L 0x05
#define REG_TX_PERMIT_LEN_H 0x06
#define REG_MAX_IDLE_LEN_L  0x07
#define REG_MAX_IDLE_LEN_H  0x08
#define REG_TX_PRE_LEN      0x09
#define REG_FILTER          0x0b
#define REG_DIV_LS_L        0x0c
#define REG_DIV_LS_H        0x0d
#define REG_DIV_HS_L        0x0e
#define REG_DIV_HS_H        0x0f
#define REG_INT_FLAG        0x10
#define REG_INT_MASK        0x11
#define REG_RX              0x14
#define REG_TX              0x15
#define REG_RX_CTRL         0x16
#define REG_TX_CTRL         0x17
#define REG_RX_ADDR         0x18
#define REG_RX_PAGE_FLAG    0x19
#define REG_FILTER_M0       0x1a    // multicast filter
#define REG_FILTER_M1       0x1b

// CDCTL01A
#define REG_CLK_CTRL        0x01
#define REG_PLL_ML          0x30
#define REG_PLL_OD_MH       0x31
#define REG_PLL_N           0x32
#define REG_PLL_CTRL        0x33
#define REG_PIN_INT_CTRL    0x34
#define REG_PIN_RE_CTRL     0x35
#define REG_CLK_STATUS      0x36

#define BIT_SETTING_TX_PUSH_PULL    (1 << 0)
#define BIT_SETTING_TX_INVERT       (1 << 1)
#define BIT_SETTING_USER_CRC        (1 << 2)
#define BIT_SETTING_NO_DROP         (1 << 3)
#define BIT_SETTING_ARBITRATE       (1 << 4)
#define BIT_SETTING_BREAK_SYNC      (1 << 5)
#define BIT_SETTING_FULL_DUPLEX     (1 << 6)

#define BIT_FLAG_BUS_IDLE           (1 << 0)
#define BIT_FLAG_RX_PENDING         (1 << 1)
#define BIT_FLAG_RX_BREAK           (1 << 2)
#define BIT_FLAG_RX_LOST            (1 << 3)
#define BIT_FLAG_RX_ERROR           (1 << 4)
#define BIT_FLAG_TX_BUF_CLEAN       (1 << 5)
#define BIT_FLAG_TX_CD              (1 << 6)
#define BIT_FLAG_TX_ERROR           (1 << 7)

#define BIT_RX_RST_POINTER          (1 << 0)
#define BIT_RX_CLR_PENDING          (1 << 1)
#define BIT_RX_CLR_LOST             (1 << 2)
#define BIT_RX_CLR_ERROR            (1 << 3)
#define BIT_RX_RST                  (1 << 4)
#define BIT_RX_CLR_BREAK            (1 << 5)
#define BIT_RX_RST_ALL              0x3f

#define BIT_TX_RST_POINTER          (1 << 0)
#define BIT_TX_START                (1 << 1)
#define BIT_TX_CLR_CD               (1 << 2)
#define BIT_TX_CLR_ERROR            (1 << 3)
#define BIT_TX_ABORT                (1 << 4)
#define BIT_TX_SEND_BREAK           (1 << 5)

#endif
