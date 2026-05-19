#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lightweight printf over UART4 */
void debug_printf(const char *fmt, ...);

/* Hex dump with label */
void debug_hex(const char *label, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif
