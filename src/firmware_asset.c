#include <stdint.h>

__asm__(
   ".section .rodata\n"
   ".global g_firmware_nvm\n"
   "g_firmware_nvm:\n"
   ".incbin \"assets/firmware.nvm\"\n"
   ".global g_firmware_nvm_end\n"
   "g_firmware_nvm_end:\n");

extern const uint8_t g_firmware_nvm[];
extern const uint8_t g_firmware_nvm_end[];
