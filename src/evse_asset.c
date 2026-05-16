#include <stdint.h>

__asm__(
   ".section .rodata\n"
   ".global g_evse_pib\n"
   "g_evse_pib:\n"
   ".incbin \"assets/evse.pib\"\n"
   ".global g_evse_pib_end\n"
   "g_evse_pib_end:\n");

extern const uint8_t g_evse_pib[];
extern const uint8_t g_evse_pib_end[];
