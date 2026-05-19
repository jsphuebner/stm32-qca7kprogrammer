#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LED_ALIVE, LED_STATEC, LED_CONTACT } led_id_t;

/* Initialise all hardware: clock, SPI1, UART4, GPIOs, SysTick */
void hw_init(void);

/* Set/clear an LED */
void led_set(led_id_t led, bool on);

/* Millisecond counter (incremented by SysTick at 1 kHz) */
uint32_t millis(void);

#ifdef __cplusplus
}
#endif
