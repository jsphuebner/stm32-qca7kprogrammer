#include "hwinit.h"
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

static volatile uint32_t g_millis = 0;

extern "C" void sys_tick_handler(void)
{
    g_millis++;
}

uint32_t millis(void)
{
    return g_millis;
}

void hw_init(void)
{
    /* System clock: HSE 8 MHz → PLL ×9 = 72 MHz */
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);

    /* Enable peripheral clocks */
    rcc_periph_clock_enable(RCC_GPIOA);
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);
    rcc_periph_clock_enable(RCC_SPI1);
    rcc_periph_clock_enable(RCC_UART4);
    rcc_periph_clock_enable(RCC_AFIO);

    /* ── SPI1 ─────────────────────────────────────────────────────────────
     * PA4 = NSS  (manual CS, output push-pull)
     * PA5 = SCK  (AF push-pull)
     * PA6 = MISO (input floating)
     * PA7 = MOSI (AF push-pull)
     */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO5 | GPIO7);
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO6);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO4);
    gpio_set(GPIOA, GPIO4); /* CS de-asserted high */

    spi_init_master(SPI1,
                    SPI_CR1_BAUDRATE_FPCLK_DIV_16,      /* 72/16 = 4.5 MHz */
                    SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE,    /* CPOL=1           */
                    SPI_CR1_CPHA_CLK_TRANSITION_2,       /* CPHA=1 → Mode 3  */
                    SPI_CR1_DFF_8BIT,
                    SPI_CR1_MSBFIRST);
    spi_enable_software_slave_management(SPI1);
    spi_set_nss_high(SPI1);
    spi_enable(SPI1);

    /* ── UART4 ────────────────────────────────────────────────────────────
     * PC10 = TX (AF push-pull), PC11 = RX (input float), 921600 baud
     */
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ,
                  GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO10);
    gpio_set_mode(GPIOC, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO11);

    usart_set_baudrate(UART4, 921600);
    usart_set_databits(UART4, 8);
    usart_set_stopbits(UART4, USART_STOPBITS_1);
    usart_set_parity(UART4, USART_PARITY_NONE);
    usart_set_flow_control(UART4, USART_FLOWCONTROL_NONE);
    usart_set_mode(UART4, USART_MODE_TX);
    usart_enable(UART4);

    /* ── LEDs ─────────────────────────────────────────────────────────────
     * PA1 = contact_out, PB4 = statec_out, PB7 = led_alive  (all off)
     */
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO1);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ,
                  GPIO_CNF_OUTPUT_PUSHPULL, GPIO4 | GPIO7);
    gpio_clear(GPIOA, GPIO1);
    gpio_clear(GPIOB, GPIO4 | GPIO7);

    /* ── SysTick at 1 kHz ─────────────────────────────────────────────── */
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB_DIV8);
    /* SysTick at 1 kHz: 72 MHz / 8 (AHB_DIV8) / 1000 Hz − 1 = 8999 */
    systick_set_reload(8999);
    systick_interrupt_enable();
    systick_counter_enable();
}

void led_set(led_id_t led, bool on)
{
    switch (led) {
    case LED_ALIVE:
        if (on) gpio_set(GPIOB, GPIO7);   else gpio_clear(GPIOB, GPIO7);
        break;
    case LED_STATEC:
        if (on) gpio_set(GPIOB, GPIO4);   else gpio_clear(GPIOB, GPIO4);
        break;
    case LED_CONTACT:
        if (on) gpio_set(GPIOA, GPIO1);   else gpio_clear(GPIOA, GPIO1);
        break;
    }
}
