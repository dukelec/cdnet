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

#ifdef __cplusplus
extern "C" {
#endif

#define local_irq_save(flags)       \
    do {                            \
        flags = _local_irq_save();  \
    } while (0)

static inline uint32_t _local_irq_save(void)
{
    uint32_t flags;
    __asm__ volatile(
        "    mrs %0, primask\n"
        "    cpsid i"
        : "=r" (flags) : : "memory", "cc");
    return flags;
}

static inline void local_irq_restore(uint32_t flags)
{
    __asm__ volatile(
        "    msr primask, %0"
        : : "r" (flags) : "memory", "cc");
}

static inline void local_irq_enable(void)
{
    __asm__ volatile("    cpsie i" : : : "memory", "cc");
}

static inline void local_irq_disable(void)
{
    __asm__ volatile("    cpsid i" : : : "memory", "cc");
}


#define irq_t   IRQn_Type

static inline void irq_enable(irq_t irq)
{
    NVIC_EnableIRQ(irq);
}

static inline void irq_disable(irq_t irq)
{
    NVIC_DisableIRQ(irq);
}


// gpio wrapper

typedef struct {
    gpio_type       *group;
    uint16_t        num;
} gpio_t;

static inline bool gpio_get_val(gpio_t *gpio)
{
    return gpio->group->idt & gpio->num;
}

static inline void gpio_set_val(gpio_t *gpio, bool value)
{
    if (value)
        gpio->group->scr = gpio->num;
    else
        gpio->group->clr = gpio->num;
}

static inline void gpio_set_high(gpio_t *gpio)
{
    gpio->group->scr = gpio->num;
}

static inline void gpio_set_low(gpio_t *gpio)
{
    gpio->group->clr = gpio->num;
}


#ifdef CD_ARCH_SPI
// spi wrapper

typedef struct {
    spi_type          *hspi;
    gpio_t            *ns_pin;
} spi_t;


static inline int HAL_SPI_Transmit(spi_type *dev, uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        while(spi_i2s_flag_get(dev, SPI_I2S_TDBE_FLAG) == RESET);
        spi_i2s_data_transmit(dev, *(buf + i));
        while(spi_i2s_flag_get(dev, SPI_I2S_RDBF_FLAG) == RESET);
        volatile uint8_t rx_dummy = spi_i2s_data_receive(dev);
        (void)rx_dummy;
    }
    return 0;
}

static inline int HAL_SPI_Receive(spi_type *dev, uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++) {
        while(spi_i2s_flag_get(dev, SPI_I2S_TDBE_FLAG) == RESET);
        spi_i2s_data_transmit(dev, 0);
        while(spi_i2s_flag_get(dev, SPI_I2S_RDBF_FLAG) == RESET);
        *(buf + i) = spi_i2s_data_receive(dev);
    }
    return 0;
}


static inline int spi_mem_write(spi_t *spi, uint8_t mem_addr, const uint8_t *buf, int len)
{
    int ret = 0;
    gpio_set_low(spi->ns_pin);
    ret = HAL_SPI_Transmit(spi->hspi, &mem_addr, 1);
    ret |= HAL_SPI_Transmit(spi->hspi, (uint8_t *)buf, len);
    gpio_set_high(spi->ns_pin);
    return ret;
}

static inline int spi_mem_read(spi_t *spi, uint8_t mem_addr, uint8_t *buf, int len)
{
    int ret = 0;
    gpio_set_low(spi->ns_pin);
    ret = HAL_SPI_Transmit(spi->hspi, &mem_addr, 1);
    ret |= HAL_SPI_Receive(spi->hspi, buf, len);
    gpio_set_high(spi->ns_pin);
    return ret;
}
#endif


#ifdef CD_ARCH_SPI_DMA
// spi wrapper

typedef struct {
    spi_type            *spi;
    gpio_t              *ns_pin;

    dma_type            *dma_rx;
    dma_channel_type    *dma_ch_rx;
    dma_channel_type    *dma_ch_tx;
    uint32_t            dma_mask; // DMA_ISR.TCIFx
    uint8_t             dummy_tx;
    uint8_t             dummy_rx;
} spi_t;


void spi_wr(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len);
void spi_wr_it(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len);
void spi_wr_init(spi_t *dev);

static inline int spi_mem_write(spi_t *spi, uint8_t mem_addr, const uint8_t *buf, int len)
{
    gpio_set_low(spi->ns_pin);
    spi_wr(spi, &mem_addr, NULL, 1);
    spi_wr(spi, buf, NULL, len);
    gpio_set_high(spi->ns_pin);
    return 0;
}

static inline int spi_mem_read(spi_t *spi, uint8_t mem_addr, uint8_t *buf, int len)
{
    gpio_set_low(spi->ns_pin);
    spi_wr(spi, &mem_addr, NULL, 1);
    spi_wr(spi, NULL, buf, len);
    gpio_set_high(spi->ns_pin);
    return 0;
}
#endif


#ifdef CD_ARCH_CRC_HW
uint16_t crc16_hw_sub(const uint8_t *data, uint32_t length, uint16_t crc_val);

static inline uint16_t crc16_hw(const uint8_t *data, uint32_t length)
{
   return crc16_hw_sub(data, length, 0xffff);
}
#endif


#ifndef CD_SYSTICK_US_DIV
#define CD_SYSTICK_US_DIV   1000
#endif

extern volatile uint32_t timebase_ticks;

static inline uint32_t get_systick(void)
{
    return timebase_ticks;
}

static inline void delay_systick(uint32_t val)
{
    uint32_t start = get_systick();
    while (get_systick() - start <= val);
}

void delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif
