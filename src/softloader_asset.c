#include <stdint.h>

__asm__(
   ".section .rodata\n"
   ".global g_softloader_nvm\n"
   "g_softloader_nvm:\n"
   ".incbin \"assets/softloader.nvm\"\n"
   ".global g_softloader_nvm_end\n"
   "g_softloader_nvm_end:\n");

extern const uint8_t g_softloader_nvm[];
extern const uint8_t g_softloader_nvm_end[];
