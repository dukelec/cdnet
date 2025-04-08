/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#ifndef __ARCH_WRAPPER_H__
#define __ARCH_WRAPPER_H__

#define local_irq_save(flags)       \
    do { } while (0)
#define local_irq_restore(flags)    \
    do { } while (0)
#define local_irq_enable()          \
    do { } while (0)
#define local_irq_disable()         \
    do { } while (0)


uint32_t get_systick(void);

#ifndef CD_SYSTICK_US_DIV
#define CD_SYSTICK_US_DIV   1000
#endif

#endif
