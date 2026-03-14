OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  . = 0x80000000;

  .text : {
    *(.text.entry)
    *(.text .text.*)
  }

  . = ALIGN(0x1000);
  .rodata : {
    *(.rodata .rodata.*)
  }

  . = ALIGN(0x1000);
  .data : {
    *(.data .data.*)
  }

  . = ALIGN(0x1000);
  .bss : {
    /* end_address 标记内核数据结束位置，kalloc 从这里开始管理空闲内存（Lab3用） */
    end_address = .;
    *(.bss .bss.*)
  }

  . = ALIGN(0x1000);
  kernel_end = .;
}
