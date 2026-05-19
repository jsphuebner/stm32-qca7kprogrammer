#include "debug.h"

#ifndef UNIT_TEST
#include <libopencm3/stm32/usart.h>

static void uart_putchar(char c)
{
    usart_send_blocking(UART4, (uint16_t)(uint8_t)c);
}

static void uart_puts(const char *s)
{
    while (*s) uart_putchar(*s++);
}

static void uart_put_uint(unsigned int v, int base)
{
    char buf[12];
    int  i = 0;
    if (v == 0) { uart_putchar('0'); return; }
    while (v) {
        int d = (int)(v % (unsigned)base);
        buf[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (unsigned)base;
    }
    while (i--) uart_putchar(buf[i]);
}

void debug_printf(const char *fmt, ...)
{
    /* Minimal %-format handler: supports %s, %u, %x, %c, %% */
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { uart_putchar(*p); continue; }
        p++;
        switch (*p) {
        case 's': uart_puts(__builtin_va_arg(ap, const char *)); break;
        case 'u': uart_put_uint(__builtin_va_arg(ap, unsigned int), 10); break;
        case 'x': uart_put_uint(__builtin_va_arg(ap, unsigned int), 16); break;
        case 'c': uart_putchar((char)__builtin_va_arg(ap, int)); break;
        case '%': uart_putchar('%'); break;
        default:  uart_putchar('%'); uart_putchar(*p); break;
        }
    }
    __builtin_va_end(ap);
}

void debug_hex(const char *label, const uint8_t *data, uint16_t len)
{
    uart_puts(label);
    uart_putchar(':');
    for (uint16_t i = 0; i < len; i++) {
        uart_putchar(' ');
        uart_put_uint(data[i], 16);
    }
    uart_putchar('\n');
}

#else  /* UNIT_TEST */

#include <stdio.h>
#include <stdarg.h>

void debug_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void debug_hex(const char *label, const uint8_t *data, uint16_t len)
{
    printf("%s:", label);
    for (uint16_t i = 0; i < len; i++)
        printf(" %02x", data[i]);
    printf("\n");
}

#endif /* UNIT_TEST */
