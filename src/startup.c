#include <stdint.h>

extern unsigned long _estack;
extern unsigned long _sidata;
extern unsigned long _sdata;
extern unsigned long _edata;
extern unsigned long _sbss;
extern unsigned long _ebss;

int main(void);
void SysTick_Handler(void);

static void Default_Handler(void)
{
   while (1)
      ;
}

void Reset_Handler(void)
{
   unsigned long* source = &_sidata;
   unsigned long* target = &_sdata;

   while (target < &_edata)
      *target++ = *source++;

   for (target = &_sbss; target < &_ebss; )
      *target++ = 0;

   main();
   Default_Handler();
}

__attribute__((section(".isr_vector")))
void (*const g_pfnVectors[])(void) = {
   (void (*)(void))(&_estack), /* Initial stack pointer */
   Reset_Handler,              /* Reset */
   Default_Handler,            /* NMI */
   Default_Handler,            /* HardFault */
   Default_Handler,            /* MemManage */
   Default_Handler,            /* BusFault */
   Default_Handler,            /* UsageFault */
   0,                          /* Reserved */
   0,                          /* Reserved */
   0,                          /* Reserved */
   0,                          /* Reserved */
   Default_Handler,            /* SVCall */
   Default_Handler,            /* Debug monitor */
   0,                          /* Reserved */
   Default_Handler,            /* PendSV */
   SysTick_Handler,            /* SysTick */
};
