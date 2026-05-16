#include "hwinit.h"

#ifndef HOST_BUILD
namespace {
volatile uint32_t g_millis = 0;

constexpr uintptr_t RCC_BASE = 0x40021000u;
constexpr uintptr_t GPIOA_BASE = 0x40010800u;
constexpr uintptr_t USART1_BASE = 0x40013800u;
constexpr uintptr_t SPI1_BASE = 0x40013000u;
constexpr uintptr_t SYSTICK_BASE = 0xE000E010u;

inline volatile uint32_t& reg32(uintptr_t address)
{
   return *(volatile uint32_t*)address;
}

constexpr uintptr_t RCC_APB2ENR = RCC_BASE + 0x18u;
constexpr uintptr_t GPIOA_CRL = GPIOA_BASE + 0x00u;
constexpr uintptr_t GPIOA_CRH = GPIOA_BASE + 0x04u;
constexpr uintptr_t GPIOA_BSRR = GPIOA_BASE + 0x10u;
constexpr uintptr_t USART1_SR = USART1_BASE + 0x00u;
constexpr uintptr_t USART1_DR = USART1_BASE + 0x04u;
constexpr uintptr_t USART1_BRR = USART1_BASE + 0x08u;
constexpr uintptr_t USART1_CR1 = USART1_BASE + 0x0Cu;
constexpr uintptr_t SPI1_CR1 = SPI1_BASE + 0x00u;
constexpr uintptr_t SYSTICK_CSR = SYSTICK_BASE + 0x00u;
constexpr uintptr_t SYSTICK_RVR = SYSTICK_BASE + 0x04u;
constexpr uintptr_t SYSTICK_CVR = SYSTICK_BASE + 0x08u;
}

extern "C" void SysTick_Handler(void)
{
   g_millis++;
}

void clock_setup(void)
{
   reg32(RCC_APB2ENR) |= (1u << 0) | (1u << 2) | (1u << 12) | (1u << 14);
}

void systick_setup(uint32_t cpu_hz)
{
   reg32(SYSTICK_RVR) = cpu_hz / 1000u - 1u;
   reg32(SYSTICK_CVR) = 0u;
   reg32(SYSTICK_CSR) = 0x07u;
}

void gpio_setup(void)
{
   uint32_t crl = reg32(GPIOA_CRL);
   crl &= ~((0xFu << 16) | (0xFu << 20) | (0xFu << 24) | (0xFu << 28));
   crl |= (0x3u << 16);
   crl |= (0xBu << 20);
   crl |= (0x4u << 24);
   crl |= (0xBu << 28);
   reg32(GPIOA_CRL) = crl;

   uint32_t crh = reg32(GPIOA_CRH);
   crh &= ~((0xFu << 4) | (0xFu << 8));
   crh |= (0xBu << 4);
   crh |= (0x4u << 8);
   reg32(GPIOA_CRH) = crh;

   reg32(GPIOA_BSRR) = (1u << 4);
}

void spi_setup(void)
{
   reg32(SPI1_CR1) = (1u << 2) | (0x3u << 3) | (1u << 8) | (1u << 9) | (1u << 6);
}

void debug_uart_setup(void)
{
   reg32(USART1_BRR) = 0x45u;
   reg32(USART1_CR1) = (1u << 13) | (1u << 3);
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
   while ((reg32(USART1_SR) & (1u << 7)) == 0u)
      ;
   reg32(USART1_DR) = (uint32_t)ch;
}

void debug_puts(const char* text)
{
   for (size_t index = 0; text[index] != '\0'; index++)
      debug_putc(text[index]);
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
#endif
