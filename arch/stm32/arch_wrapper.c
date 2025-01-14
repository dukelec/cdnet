/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <d@d-l.io>
 */

#include "cd_utils.h"
#include "arch_wrapper.h"


#ifdef ARCH_SPI_DMA

void spi_wr(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len)
{
    DMA_TypeDef *dma = dev->dma_rx;
    DMA_Channel_TypeDef *dma_r = dev->dma_ch_rx;
    DMA_Channel_TypeDef *dma_t = dev->dma_ch_tx;
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
    DMA_Channel_TypeDef *dma_r = dev->dma_ch_rx;
    DMA_Channel_TypeDef *dma_t = dev->dma_ch_tx;

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


#ifdef ARCH_CRC_HW

uint16_t crc16_hw_sub(const uint8_t *data, uint32_t length, uint16_t crc_val)
{
    uint16_t ret_val;
#ifdef CRC_HW_IRQ_SAFE
    uint32_t flags;
    local_irq_save(flags);
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
#ifdef CRC_HW_IRQ_SAFE
    local_irq_restore(flags);
#endif
    return ret_val;
}

#endif

