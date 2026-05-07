SECTIONS
{
    /* 强行规定起始虚拟地址为 0 */
    . = 0x0;
    
    .text : {
        *(.text.entry)
        *(.text .text.*)
    }
    
    /* 必须加这个！否则 "Hello OS!" 字符串会被丢弃或放错位置 */
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