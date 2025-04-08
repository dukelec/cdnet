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

#define CDREG_VERSION               0x00
#define CDREG_CLK_CTRL              0x01
#define CDREG_SETTING               0x02
#define CDREG_IDLE_WAIT_LEN         0x04
#define CDREG_TX_PERMIT_LEN_L       0x05
#define CDREG_TX_PERMIT_LEN_H       0x06
#define CDREG_MAX_IDLE_LEN_L        0x07
#define CDREG_MAX_IDLE_LEN_H        0x08
#define CDREG_TX_PRE_LEN            0x09
#define CDREG_FILTER                0x0b
#define CDREG_DIV_LS_L              0x0c
#define CDREG_DIV_LS_H              0x0d
#define CDREG_DIV_HS_L              0x0e
#define CDREG_DIV_HS_H              0x0f
#define CDREG_INT_FLAG              0x10
#define CDREG_INT_MASK              0x11
#define CDREG_RX                    0x14
#define CDREG_TX                    0x15
#define CDREG_RX_CTRL               0x16
#define CDREG_TX_CTRL               0x17
#define CDREG_RX_ADDR               0x18
#define CDREG_RX_PAGE_FLAG          0x19
#define CDREG_FILTER_M0             0x1a
#define CDREG_FILTER_M1             0x1b
#define CDREG_PLL_ML                0x30
#define CDREG_PLL_OD_MH             0x31
#define CDREG_PLL_N                 0x32
#define CDREG_PLL_CTRL              0x33
#define CDREG_PIN_INT_CTRL          0x34
#define CDREG_PIN_RE_CTRL           0x35
#define CDREG_CLK_STATUS            0x36

#define CDBIT_SETTING_TX_PUSH_PULL  (1 << 0)
#define CDBIT_SETTING_TX_INVERT     (1 << 1)
#define CDBIT_SETTING_USER_CRC      (1 << 2)
#define CDBIT_SETTING_NO_DROP       (1 << 3)
#define CDBIT_SETTING_ARBITRATE     (1 << 4)
#define CDBIT_SETTING_BREAK_SYNC    (1 << 5)
#define CDBIT_SETTING_FULL_DUPLEX   (1 << 6)

#define CDBIT_FLAG_BUS_IDLE         (1 << 0)
#define CDBIT_FLAG_RX_PENDING       (1 << 1)
#define CDBIT_FLAG_RX_BREAK         (1 << 2)
#define CDBIT_FLAG_RX_LOST          (1 << 3)
#define CDBIT_FLAG_RX_ERROR         (1 << 4)
#define CDBIT_FLAG_TX_BUF_CLEAN     (1 << 5)
#define CDBIT_FLAG_TX_CD            (1 << 6)
#define CDBIT_FLAG_TX_ERROR         (1 << 7)

#define CDBIT_RX_RST_POINTER        (1 << 0)
#define CDBIT_RX_CLR_PENDING        (1 << 1)
#define CDBIT_RX_RST                (1 << 4)
#define CDBIT_RX_RST_ALL            (CDBIT_RX_RST | CDBIT_RX_RST_POINTER)

#define CDBIT_TX_RST_POINTER        (1 << 0)
#define CDBIT_TX_START              (1 << 1)
#define CDBIT_TX_ABORT              (1 << 4)
#define CDBIT_TX_SEND_BREAK         (1 << 5)

#endif
