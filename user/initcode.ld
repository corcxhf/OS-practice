/* user/initcode.ld */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x0;
    
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }
    
    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }
    
    .data : {
        *(.data .data.*)
    }
    
    .bss : {
        *(.bss .bss.*)
    }
}