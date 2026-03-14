    .section .text
    .global  _entry

_entry:
    la       sp, stack0

    li       a0, 4096

    add      sp, sp, a0

    call     start_main

spin:
    j        spin

    .section .bss
    .align   16

stack0:
    .space   4096