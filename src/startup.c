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
uintptr_t const g_pfnVectors[] = {
   (uintptr_t)(&_estack),          /* Initial stack pointer */
   (uintptr_t)Reset_Handler,       /* Reset */
   (uintptr_t)Default_Handler,     /* NMI */
   (uintptr_t)Default_Handler,     /* HardFault */
   (uintptr_t)Default_Handler,     /* MemManage */
   (uintptr_t)Default_Handler,     /* BusFault */
   (uintptr_t)Default_Handler,     /* UsageFault */
   (uintptr_t)0,                   /* Reserved */
   (uintptr_t)0,                   /* Reserved */
   (uintptr_t)0,                   /* Reserved */
   (uintptr_t)0,                   /* Reserved */
   (uintptr_t)Default_Handler,     /* SVCall */
   (uintptr_t)Default_Handler,     /* Debug monitor */
   (uintptr_t)0,                   /* Reserved */
   (uintptr_t)Default_Handler,     /* PendSV */
   (uintptr_t)SysTick_Handler,     /* SysTick */
};
