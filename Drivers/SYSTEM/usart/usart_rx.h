#ifndef USART_RX_H
#define USART_RX_H

#include <stdint.h>

uint8_t usart_rx_read_byte(uint8_t *data);
void usart_rx_reset(void);
uint32_t usart_rx_get_dropped_count(void);

#endif
