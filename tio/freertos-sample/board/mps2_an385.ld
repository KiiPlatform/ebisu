/* The MPS2 AN385 board as QEMU emulates it: 4 MB of SSRAM at 0, which QEMU
   loads the program into, and another 4 MB at 0x20000000, for its data, the
   heap and the stack main() starts on. */
MEMORY
{
    CODE (rx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rw) : ORIGIN = 0x20000000, LENGTH = 4M
}

ENTRY(reset_handler)

/* main()'s until the scheduler starts, then the interrupts'. Each task has a
   stack of its own, from the heap. */
STACK_SIZE = 8K;

SECTIONS
{
    .text :
    {
        KEEP(*(.vectors))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > CODE

    .ARM.exidx :
    {
        *(.ARM.exidx .ARM.exidx.*)
    } > CODE

    /* Copied to RAM by reset_handler(). */
    .data :
    {
        . = ALIGN(4);
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > RAM AT > CODE
    __data_load = LOADADDR(.data);

    /* Zeroed by reset_handler(). */
    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start = .;
        . = ORIGIN(RAM) + LENGTH(RAM) - STACK_SIZE;
        __heap_end = .;
    } > RAM

    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
