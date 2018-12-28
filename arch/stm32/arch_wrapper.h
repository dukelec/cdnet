/*
 * Software License Agreement (MIT License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __ARCH_WRAPPER_H__
#define __ARCH_WRAPPER_H__

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


// gpio wrapper

typedef struct {
    GPIO_TypeDef    *group;
    uint16_t        num;
} gpio_t;

static inline bool gpio_get_value(gpio_t *gpio)
{
    return HAL_GPIO_ReadPin(gpio->group, gpio->num);
}

static inline void gpio_set_value(gpio_t *gpio, bool value)
{
    HAL_GPIO_WritePin(gpio->group, gpio->num, value);
}


// uart wrapper

typedef struct {
    UART_HandleTypeDef *huart;
} uart_t;


#ifdef ARCH_SPI
// spi wrapper

typedef struct {
    SPI_HandleTypeDef *hspi;
    gpio_t            *ns_pin;
} spi_t;

static inline int spi_mem_write(spi_t *spi, uint8_t mem_addr,
        const uint8_t *buf, int len)
{
    int ret = 0;
    gpio_set_value(spi->ns_pin, 0);
    ret = HAL_SPI_Transmit(spi->hspi, &mem_addr, 1, HAL_MAX_DELAY);
    ret = HAL_SPI_Transmit(spi->hspi, (uint8_t *)buf, len, HAL_MAX_DELAY);
    gpio_set_value(spi->ns_pin, 1);
    return ret;
}

static inline int spi_mem_read(spi_t *spi, uint8_t mem_addr,
        uint8_t *buf, int len)
{
    int ret = 0;
    gpio_set_value(spi->ns_pin, 0);
    ret = HAL_SPI_Transmit(spi->hspi, &mem_addr, 1, HAL_MAX_DELAY);
    ret = HAL_SPI_Receive(spi->hspi, buf, len, HAL_MAX_DELAY);
    gpio_set_value(spi->ns_pin, 1);
    return ret;
}

static inline int spi_dma_write(spi_t *spi, const uint8_t *buf, int len)
{
    return HAL_SPI_Transmit_DMA(spi->hspi, (uint8_t *)buf, len);
}

static inline int spi_dma_read(spi_t *spi, uint8_t *buf, int len)
{
    return HAL_SPI_Receive_DMA(spi->hspi, buf, len);
}

static inline int spi_dma_write_read(spi_t *spi, const uint8_t *wr_buf,
        uint8_t *rd_buf, int len)
{
    return HAL_SPI_TransmitReceive_DMA(spi->hspi,
            (uint8_t *)wr_buf, rd_buf, len);
}

#endif


#ifdef ARCH_I2C
// i2c wrapper

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t            dev_addr; // 8 bit, equal to i2c write address
} i2c_t;

static inline int i2c_mem_write(i2c_t *i2c, uint8_t mem_addr,
        const uint8_t *buf, int len)
{
    return HAL_I2C_Mem_Write(i2c->hi2c, i2c->dev_addr, mem_addr, 1,
            (uint8_t *)buf, len, HAL_MAX_DELAY);
}

static inline int i2c_mem_read(i2c_t *i2c, uint8_t mem_addr,
        uint8_t *buf, int len)
{
    return HAL_I2C_Mem_Read(i2c->hi2c, i2c->dev_addr, mem_addr, 1,
            buf, len, HAL_MAX_DELAY);
}
#endif


#ifndef SYSTICK_US_DIV
#define SYSTICK_US_DIV  1000
#endif

static inline uint32_t get_systick(void)
{
    return HAL_GetTick();
}

static inline void delay_systick(uint32_t val)
{
    HAL_Delay(val);
}

#endif

