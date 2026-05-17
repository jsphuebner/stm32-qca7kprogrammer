#ifndef STM32_QCA7KPROGRAMMER_HWINIT_H
#define STM32_QCA7KPROGRAMMER_HWINIT_H

#include <stdint.h>

void clock_setup(void);
void systick_setup(uint32_t cpu_hz);
void gpio_setup(void);
void spi_setup(void);
void debug_uart_setup(void);
void delay_ms(uint32_t value);
uint32_t millis(void);
void debug_putc(char ch);
void debug_puts(const char* text);
void status_running_light_update(void);
void led_set_all(bool on);

#endif
