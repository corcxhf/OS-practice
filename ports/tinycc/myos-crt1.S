.section .text
.global _start
.type _start, @function

_start:
    call main
    li a7, 2
    ecall

1:
    j 1b
