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

#define CSR_MSTATUS     0x300
#define SR_MIE          0x00000008
#define CSR_STATUS      CSR_MSTATUS
#define SR_IE           SR_MIE
#define __ASM_STR(x)    #x


#define _csr_set(csr, val)                               \
({                                                       \
    unsigned long __v = (unsigned long)(val);            \
    __asm__ __volatile__ ("csrs " __ASM_STR(csr) ", %0"  \
                  : : "rK" (__v)                         \
                  : "memory");                           \
})

#define _csr_read_clear(csr, val)                               \
({                                                              \
    unsigned long __v = (unsigned long)(val);                   \
    __asm__ __volatile__ ("csrrc %0, " __ASM_STR(csr) ", %1"    \
                  : "=r" (__v) : "rK" (__v)                     \
                  : "memory");                                  \
    __v;                                                        \
})

#define _csr_clear(csr, val)                            \
({                                                      \
    unsigned long __v = (unsigned long)(val);           \
    __asm__ __volatile__ ("csrc " __ASM_STR(csr) ", %0" \
                  : : "rK" (__v)                        \
                  : "memory");                          \
})


#define local_irq_save(flags)       \
    do {                            \
        flags = _local_irq_save();  \
    } while (0)

static inline uint32_t _local_irq_save(void)
{
    return _csr_read_clear(CSR_STATUS, SR_IE);
}

static inline void local_irq_restore(uint32_t flags)
{
    _csr_set(CSR_STATUS, flags & SR_IE);
}

static inline void local_irq_enable(void)
{
    _csr_set(CSR_STATUS, SR_IE);
}

static inline void local_irq_disable(void)
{
    _csr_clear(CSR_STATUS, SR_IE);
}


#ifdef CD_IRQ_SAFE

#define irq_t   uint32_t

static inline void irq_enable(irq_t irq)
{
    gpio_intr_enable(irq);
}

static inline void irq_disable(irq_t irq)
{
    gpio_intr_disable(irq);
}
#endif


// gpio wrapper

#define gpio_t  uint32_t

// chips with no more than 32 gpios (e.g. esp32c3) do not define GPIO_*1_REG

static inline bool gpio_get_val(gpio_t *gpio)
{
#ifdef GPIO_IN1_REG
    if (*gpio < 32)
#endif
        return REG_GET_BIT(GPIO_IN_REG, BIT(*gpio));
#ifdef GPIO_IN1_REG
    else
        return REG_GET_BIT(GPIO_IN1_REG, BIT(*gpio));
#endif
}

static inline void gpio_set_high(gpio_t *gpio)
{
#ifdef GPIO_IN1_REG
    if (*gpio < 32)
#endif
        REG_WRITE(GPIO_OUT_W1TS_REG, BIT(*gpio));
#ifdef GPIO_IN1_REG
    else
        REG_WRITE(GPIO_OUT1_W1TS_REG, BIT(*gpio));
#endif
}

static inline void gpio_set_low(gpio_t *gpio)
{
#ifdef GPIO_IN1_REG
    if (*gpio < 32)
#endif
        REG_WRITE(GPIO_OUT_W1TC_REG, BIT(*gpio));
#ifdef GPIO_IN1_REG
    else
        REG_WRITE(GPIO_OUT1_W1TC_REG, BIT(*gpio));
#endif
}

static inline void gpio_set_val(gpio_t *gpio, bool value)
{
    if (value)
        gpio_set_high(gpio);
    else
        gpio_set_low(gpio);
}


#if defined(CD_ARCH_SPI) || defined(CD_ARCH_SPI_DMA)
// spi wrapper

typedef struct {
    spi_device_handle_t dev;
    spi_transaction_t   trans;
    gpio_t              *ns_pin;
} spi_t;


void spi_wr(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len);
void spi_wr_it(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len);
static inline void spi_wr_init(spi_t *dev) {}


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


static inline uint32_t get_systick(void)
{
    return esp_log_timestamp();
}

static inline void delay_systick(uint32_t val)
{
    esp_rom_delay_us(val * 1000);
}

static inline void delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

#ifdef __cplusplus
}
#endif

#endif
