/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cd_utils.h"


#ifdef CD_ARCH_SPI_DMA

#define CCR                 ctrl
#define CNDTR               dtcnt
#define CMAR                maddr
#define CPAR                paddr
#define ISR                 sts
#define IFCR                clr
#define DMA_CCR_EN          (1 << 0)
#define DMA_CCR_TCIE        (1 << 1)
#define DMA_CCR_MINC        (1 << 7)

#define SR                  sts
#define DR                  dt
#define CR1                 ctrl1
#define CR2                 ctrl2
#define SPI_FLAG_BSY        (1 << 7)
#define SPI_CR1_SPE         (1 << 6)
#define SPI_CR2_RXDMAEN     (1 << 0)
#define SPI_CR2_TXDMAEN     (1 << 1)

#define SET_BIT(REG, BIT)   ((REG) |= (BIT))


void spi_wr(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len)
{
    dma_type *dma = dev->dma_rx;
    dma_channel_type *dma_r = dev->dma_ch_rx;
    dma_channel_type *dma_t = dev->dma_ch_tx;
    uint32_t mask = dev->dma_mask;

    dma_r->CCR &= ~DMA_CCR_EN;
    dma_r->CCR &= ~(DMA_CCR_MINC | DMA_CCR_TCIE);
    dma_r->CNDTR = len;
    dma_r->CMAR = (uint32_t)(r_buf ? r_buf : &dev->dummy_rx);
    dma_r->CCR |= r_buf ? (DMA_CCR_EN | DMA_CCR_MINC) : DMA_CCR_EN;

    dma_t->CCR &= ~DMA_CCR_EN;
    dma_t->CCR &= ~DMA_CCR_MINC;
    dma_t->CNDTR = len;
    dma_t->CMAR = (uint32_t)(w_buf ? w_buf : &dev->dummy_tx);
    dma_t->CCR |= w_buf ? (DMA_CCR_EN | DMA_CCR_MINC) : DMA_CCR_EN;

    while (!(dma->ISR & mask));
    dma->IFCR = mask;
}

void spi_wr_it(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len)
{
    dma_channel_type *dma_r = dev->dma_ch_rx;
    dma_channel_type *dma_t = dev->dma_ch_tx;

    dma_r->CCR &= ~DMA_CCR_EN;
    dma_r->CCR &= ~DMA_CCR_MINC;
    dma_r->CNDTR = len;
    dma_r->CMAR = (uint32_t)(r_buf ? r_buf : &dev->dummy_rx);
    dma_r->CCR |= r_buf ? (DMA_CCR_EN | DMA_CCR_MINC | DMA_CCR_TCIE) : (DMA_CCR_EN | DMA_CCR_TCIE);

    dma_t->CCR &= ~DMA_CCR_EN;
    dma_t->CCR &= ~DMA_CCR_MINC;
    dma_t->CNDTR = len;
    dma_t->CMAR = (uint32_t)(w_buf ? w_buf : &dev->dummy_tx);
    dma_t->CCR |= w_buf ? (DMA_CCR_EN | DMA_CCR_MINC) : DMA_CCR_EN;
}


/* isr template:
void spi_wr_isr(void)
{
    uint32_t flag_it = spi_dev.dma_rx->ISR;
    if (flag_it & spi_dev.dma_mask) {
        spi_dev.dma_rx->IFCR = spi_dev.dma_mask;
        user_callback();
    }
}
*/

void spi_wr_init(spi_t *dev)
{
    SET_BIT(dev->spi->CR1, SPI_CR1_SPE); // enable spi
    SET_BIT(dev->spi->CR2, SPI_CR2_RXDMAEN);
    SET_BIT(dev->spi->CR2, SPI_CR2_TXDMAEN);
    dev->dma_ch_rx->CCR &= ~DMA_CCR_EN;
    dev->dma_ch_tx->CCR &= ~DMA_CCR_EN;
    dev->dma_ch_rx->CPAR = (uint32_t)&dev->spi->DR;
    dev->dma_ch_tx->CPAR = (uint32_t)&dev->spi->DR;
}

#endif


#ifdef CD_ARCH_CRC_HW

#define INIT    idt
#define CR      ctrl
#ifndef DR
#define DR      dt
#endif

#ifdef CD_ARCH_CRC_HW_IT
static cd_spinlock_t crc_lock = {0};
#endif


uint16_t crc16_hw_sub(const uint8_t *data, uint32_t length, uint16_t crc_val)
{
    uint16_t ret_val;
#ifdef CD_ARCH_CRC_HW_IT // not recommended, avoid large critical sections
    uint32_t flags;
    cd_irq_save(&crc_lock, flags);
#endif
    CRC->INIT = crc_val;
    CRC->CR = 0xe9;
    CRC->INIT = CRC->DR; // bit-reverse crc_val

    while (((unsigned)data & 3) && length) {
        *(volatile uint8_t *)&CRC->DR = *data++;
        length--;
    }

    unsigned cnt = length >> 2;
    while (cnt--) {
        CRC->DR = *(uint32_t *)data;
        data += 4;
    }

    length &= 3;
    while (length--)
        *(volatile uint8_t *)&CRC->DR = *data++;

    ret_val = CRC->DR;
#ifdef CD_ARCH_CRC_HW_IT
    cd_irq_restore(&crc_lock, flags);
#endif
    return ret_val;
}

#endif


void delay_us(uint32_t us)
{
    uint32_t cnt_1ms = SysTick->LOAD + 1;
    uint32_t last = SysTick->VAL;
    uint32_t total = 0;
    uint32_t target = cnt_1ms / 1000 * us;

    while (total <= target) {
        uint32_t cur = SysTick->VAL;
        int32_t diff = last - cur;
        if (diff < 0)
            diff += cnt_1ms;
        total += diff;
        last = cur;
    }
}

