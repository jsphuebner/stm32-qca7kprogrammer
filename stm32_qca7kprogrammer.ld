MEMORY
{
   rom (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
   ram (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(ram) + LENGTH(ram);

SECTIONS
{
   .isr_vector :
   {
      KEEP(*(.isr_vector))
   } > rom

   .text :
   {
      *(.text*)
      *(.rodata*)
      *(.glue_7)
      *(.glue_7t)
      *(.eh_frame*)
   } > rom

   _sidata = LOADADDR(.data);
   .data :
   {
      _sdata = .;
      *(.data*)
      _edata = .;
   } > ram AT > rom

   .bss :
   {
      _sbss = .;
      *(.bss*)
      *(COMMON)
      _ebss = .;
   } > ram
}
