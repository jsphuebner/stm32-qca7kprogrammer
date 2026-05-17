#include "hwinit.h"
#include <stddef.h>

#ifndef HOST_BUILD
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>
namespace {
volatile uint32_t g_millis = 0;
constexpr uint16_t kLedPinsMask = GPIO2 | GPIO10 | GPIO11;

void set_led_mask(uint32_t on_mask)
{
   const uint16_t set_bits = (uint16_t)(kLedPinsMask & (uint16_t)on_mask);
   const uint16_t clear_bits = (uint16_t)(kLedPinsMask & (uint16_t)~on_mask);
   gpio_set(GPIOB, set_bits);
   gpio_clear(GPIOB, clear_bits);
}
}

extern "C" void SysTick_Handler(void)
{
   g_millis++;
}

void clock_setup(void)
{
   rcc_periph_clock_enable(RCC_AFIO);
   rcc_periph_clock_enable(RCC_GPIOA);
   rcc_periph_clock_enable(RCC_GPIOB);
   rcc_periph_clock_enable(RCC_GPIOC);
   rcc_periph_clock_enable(RCC_UART4);
   rcc_periph_clock_enable(RCC_SPI1);
}

void systick_setup(uint32_t cpu_hz)
{
   systick_set_reload(cpu_hz / 1000u - 1u);
   systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
   systick_clear();
   systick_interrupt_enable();
   systick_counter_enable();
}

void gpio_setup(void)
{
   gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, kLedPinsMask);
   gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);
   gpio_set_mode(GPIOA,
                 GPIO_MODE_OUTPUT_50_MHZ,
                 GPIO_CNF_OUTPUT_ALTFN_PUSHPULL,
                 GPIO5 | GPIO7);
   gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO6);
   gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO10);
   gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO11);
   gpio_clear(GPIOB, kLedPinsMask);
   gpio_set(GPIOA, GPIO4);
}

void spi_setup(void)
{
   spi_reset(SPI1);
   spi_init_master(SPI1,
                   SPI_CR1_BAUDRATE_FPCLK_DIV_16,
                   SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
                   SPI_CR1_CPHA_CLK_TRANSITION_1,
                   SPI_CR1_DFF_8BIT,
                   SPI_CR1_MSBFIRST);
   spi_enable_software_slave_management(SPI1);
   spi_set_nss_high(SPI1);
   spi_enable(SPI1);
}

void debug_uart_setup(void)
{
   usart_set_baudrate(UART4, 921600u);
   usart_set_databits(UART4, 8u);
   usart_set_stopbits(UART4, USART_STOPBITS_1);
   usart_set_parity(UART4, USART_PARITY_NONE);
   usart_set_flow_control(UART4, USART_FLOWCONTROL_NONE);
   usart_set_mode(UART4, USART_MODE_TX);
   usart_enable(UART4);
}

void delay_ms(uint32_t value)
{
   const uint32_t end = g_millis + value;
   while ((int32_t)(end - g_millis) > 0)
      __asm__ volatile("nop");
}

uint32_t millis(void)
{
   return g_millis;
}

void debug_putc(char ch)
{
   usart_send_blocking(UART4, (uint16_t)(uint8_t)ch);
}

void debug_puts(const char* text)
{
   for (size_t index = 0; text[index] != '\0'; index++)
      debug_putc(text[index]);
}

void status_running_light_update(void)
{
   static uint32_t next_update = 0u;
   static uint32_t phase = 0u;

   if ((int32_t)(g_millis - next_update) < 0)
      return;

   next_update = g_millis + 120u;
   set_led_mask(1u << phase);
   phase = (phase + 1u) % 3u;
}

void led_set_all(bool on)
{
   set_led_mask(on ? kLedPinsMask : 0u);
}
#else
void clock_setup(void) {}
void systick_setup(uint32_t) {}
void gpio_setup(void) {}
void spi_setup(void) {}
void debug_uart_setup(void) {}
void delay_ms(uint32_t) {}
uint32_t millis(void) { return 0u; }
void debug_putc(char) {}
void debug_puts(const char*) {}
void status_running_light_update(void) {}
void led_set_all(bool) {}
#endif
