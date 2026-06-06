#define SYS_fork 1
#define SYS_exit 2
#define SYS_wait 3
#define SYS_getpid 11
#define SYS_sbrk 12
#define SYS_exec 13

#define SYS_open 15
#define SYS_write 16
#define SYS_read 17
#define SYS_close 18
    .global getpid
getpid:
    li      a7, SYS_getpid
    ecall
    ret

    .global exit
exit:
    li      a7, SYS_exit
    ecall
    ret

    .global write
write:
    li      a7, SYS_write
    ecall
    ret

    .global fork
fork:
    li      a7, SYS_fork
    ecall
    ret

    .global wait
wait:
    li      a7, SYS_wait
    ecall
    ret
