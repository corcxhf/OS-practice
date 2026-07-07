.section .text
.global _start
.type _start, @function

_start:
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s2
    call __myos_init_environ
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call main
    li a7, 2
    ecall

1:
    j 1b
