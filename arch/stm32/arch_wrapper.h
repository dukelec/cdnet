/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2017, DUKELEC, Inc.
 * All rights reserved.
 *
 * Author: Duke Fong <duke@dukelec.com>
 */

#ifndef __ARCH_WRAPPER_H__
#define __ARCH_WRAPPER_H__

#include "common.h"
#include "stm32f1xx_hal.h"
#include "stm32f103xb.h"


#define local_irq_save(flags)       \
    do {                            \
        flags = _local_irq_save();  \
    } while (0)

static inline uint32_t _local_irq_save(void)
{
    uint32_t flags;
    asm volatile(
        "    mrs %0, primask\n"
        "    cpsid i"
        : "=r" (flags) : : "memory", "cc");
    return flags;
}

static inline void local_irq_restore(uint32_t flags)
{
    asm volatile(
        "    msr primask, %0"
        : : "r" (flags) : "memory", "cc");
}

static inline void local_irq_enable(void)
{
    asm volatile("    cpsie i" : : : "memory", "cc");
}

static inline void local_irq_disable(void)
{
    asm volatile("    cpsid i" : : : "memory", "cc");
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

static inline void gpio_set_irq(gpio_t *gpio, bool is_enable)
{
}


// uart wrapper

typedef struct {
    UART_HandleTypeDef *huart;
} uart_t;

static inline void uart_receive_flush(uart_t *uart)
{
    if (uart->huart->Instance->SR & USART_SR_RXNE) { // avoid ORE error
        volatile int tmp = uart->huart->Instance->DR;
        (void)tmp; // suppress warning
    }
}

static inline int uart_receive_it(uart_t *uart, uint8_t *buf, uint16_t len)
{
    return HAL_UART_Receive_IT(uart->huart, buf, len);
}

static inline int uart_abort_receive_it(uart_t *uart)
{
    return HAL_UART_AbortReceive_IT(uart->huart);
}

static inline int uart_receive(uart_t *uart, uint8_t *buf, uint16_t len)
{
    // not block, so len should be 1
    return HAL_UART_Receive(uart->huart, buf, len, 0 /* timeout */);
}

static inline int uart_transmit_it(uart_t *uart, const uint8_t *buf, uint16_t len)
{
    return HAL_UART_Transmit_IT(uart->huart, (uint8_t *)buf, len);
}

static inline int uart_transmit_is_ready(uart_t *uart)
{
    return uart->huart->gState == HAL_UART_STATE_READY;
}

static inline int uart_transmit(uart_t *uart, const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        while (!__HAL_UART_GET_FLAG(uart->huart, UART_FLAG_TXE));
        uart->huart->Instance->DR = *(buf + i);
    }
    return 0;
}

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


static inline uint32_t get_systick(void)
{
    return HAL_GetTick();
}

#endif

