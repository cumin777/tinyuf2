/*
*****************************************************************************
**
**  Linker script for STM32C5A3xx Bootloader
**  1MB FLASH, 256KB RAM
**
**  Adapted for new CMSIS .c startup file (startup_stm32c5a3xx.c)
**  which expects __INITIAL_SP, __STACK_LIMIT, .vectors section, etc.
**
*****************************************************************************
*/

__flash_boot_size = DEFINED(__flash_boot_size) ? __flash_boot_size : 24K;
__ram_size = DEFINED(__ram_size) ? __ram_size : 256K;

/* Entry Point */
ENTRY(Reset_Handler)

HEAP_SIZE = 0x200;
STACK_SIZE = 0x400;

/* Specify the memory areas */
MEMORY
{
  RAM (xrw)     : ORIGIN = 0x20000000, LENGTH = __ram_size - 4         /* reserve 4 bytes for double tap */
  FLASH (rx)    : ORIGIN = 0x08000000, LENGTH = __flash_boot_size - 1K /* bootloader code   */
  CONFIG (rx)   : ORIGIN = 0x08000000 + __flash_boot_size - 1K, LENGTH = 1K  /* 1 KByte for cf2 config */
}

/* Double tap at top of RAM (after stack) */
_board_dfu_dbl_tap = ORIGIN(RAM) + LENGTH(RAM);

/* Define output sections */
SECTIONS
{
  /* Vector table - startup_stm32c5a3xx.c uses .vectors section */
  .vectors :
  {
    . = ALIGN(8);
    KEEP(*(.vectors))
    . = ALIGN(8);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(8);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(8);
    _etext = .;
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(8);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(8);
  } >FLASH

  .ARM.extab (READONLY) :
  {
    . = ALIGN(8);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(8);
  } >FLASH

  .ARM (READONLY) :
  {
    . = ALIGN(8);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(8);
  } >FLASH

  .preinit_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(8);
  } >FLASH

  .init_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(8);
  } >FLASH

  .copy.table (READONLY) :
  {
    . = ALIGN(8);
    __copy_table_start__ = .;
    LONG(LOADADDR(.data));
    LONG(ADDR(.data));
    LONG(SIZEOF(.data) / 4);
    __copy_table_end__ = .;
  } >FLASH

  .zero.table (READONLY) :
  {
    . = ALIGN(8);
    __zero_table_start__ = .;
    LONG(ADDR(.bss));
    LONG(SIZEOF(.bss) / 4);
    __zero_table_end__ = .;
  } >FLASH

  .fini_array (READONLY) :
  {
    . = ALIGN(8);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(8);
  } >FLASH

  /* CF2 config */
  . = ORIGIN(CONFIG);
  .config :
  {
    KEEP (*(.config))
  } >CONFIG

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(8);
    _sdata = .;
    __data_start__ = .;
    *(.data)
    *(.data*)
    . = ALIGN(8);
    _edata = .;
  } >RAM AT> FLASH

  /* Uninitialized data section */
  .bss :
  {
    . = ALIGN(8);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(8);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* Heap */
  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    __end__ = .;
    PROVIDE (end = .);
    _heap_start = .;
    . += HEAP_SIZE;
    . = ALIGN(8);
    _heap_end = .;
    __HeapLimit = .;
  } >RAM

  /* Stack - placed at top of RAM. Provides symbols for CMSIS startup. */
  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    __STACK_LIMIT = .;
    . += STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
    __stack = .;
  } >RAM

  /* Provide __INITIAL_SP = top of RAM (above stack, for vector table) */
  __INITIAL_SP = ORIGIN(RAM) + LENGTH(RAM);

  /* _estack also at top of RAM for compatibility */
  _estack = ORIGIN(RAM) + LENGTH(RAM);

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
