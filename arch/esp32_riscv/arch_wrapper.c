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

void spi_wr(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len)
{
    spi_transaction_t t = {
        .length = 8 * len,
        .tx_buffer = w_buf,
        .rx_buffer = r_buf
    };
    esp_err_t ret = spi_device_polling_transmit(dev->dev, &t);
    assert(ret == ESP_OK);
}

void spi_wr_it(spi_t *dev, const uint8_t *w_buf, uint8_t *r_buf, int len)
{
    memset(&dev->trans, 0, sizeof(spi_transaction_t));
    dev->trans.length = 8 * len;
    dev->trans.tx_buffer = w_buf;
    dev->trans.rx_buffer = r_buf;
    esp_err_t ret = spi_device_queue_trans(dev->dev, &dev->trans, portMAX_DELAY);
    assert(ret == ESP_OK);
}


/* isr template:
void spi_wr_isr(spi_transaction_t *t)
{
    if (t == &dev->trans)
        user_callback();
}
*/

#endif

