
user/_echo:     file format elf64-littleriscv


Disassembly of section .text:

0000000000000000 <put_str>:
#include "proc.h"
#include "types.h"

void put_str(const char *s)
{
   0:	7179                	addi	sp,sp,-48
   2:	f422                	sd	s0,40(sp)
   4:	1800                	addi	s0,sp,48
   6:	fca43c23          	sd	a0,-40(s0)
    int len = 0;
   a:	fe042623          	sw	zero,-20(s0)
    while (s[len])
   e:	a031                	j	1a <put_str+0x1a>
        len++;
  10:	fec42783          	lw	a5,-20(s0)
  14:	2785                	addiw	a5,a5,1
  16:	fef42623          	sw	a5,-20(s0)
    while (s[len])
  1a:	fec42783          	lw	a5,-20(s0)
  1e:	fd843703          	ld	a4,-40(s0)
  22:	97ba                	add	a5,a5,a4
  24:	0007c783          	lbu	a5,0(a5)
  28:	f7e5                	bnez	a5,10 <put_str+0x10>

    register uint64 a0_asm __asm__("a0") = 1;
  2a:	4505                	li	a0,1
    register uint64 a1_asm __asm__("a1") = (uint64)s;
  2c:	fd843783          	ld	a5,-40(s0)
  30:	85be                	mv	a1,a5
    register uint64 a2_asm __asm__("a2") = len;
  32:	fec42783          	lw	a5,-20(s0)
  36:	863e                	mv	a2,a5
    register uint64 a7_asm __asm__("a7") = SYS_write;
  38:	48c1                	li	a7,16
    __asm__ volatile("ecall" : : "r"(a0_asm), "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
  3a:	00000073          	ecall
}
  3e:	0001                	nop
  40:	7422                	ld	s0,40(sp)
  42:	6145                	addi	sp,sp,48
  44:	8082                	ret

0000000000000046 <main>:

int main(int argc, char *argv[])
{
  46:	7179                	addi	sp,sp,-48
  48:	f406                	sd	ra,40(sp)
  4a:	f022                	sd	s0,32(sp)
  4c:	1800                	addi	s0,sp,48
  4e:	87aa                	mv	a5,a0
  50:	fcb43823          	sd	a1,-48(s0)
  54:	fcf42e23          	sw	a5,-36(s0)
    for (int i = 1; i < argc; i++)
  58:	4785                	li	a5,1
  5a:	fef42623          	sw	a5,-20(s0)
  5e:	a835                	j	9a <main+0x54>
    {
        put_str(argv[i]);
  60:	fec42783          	lw	a5,-20(s0)
  64:	078e                	slli	a5,a5,0x3
  66:	fd043703          	ld	a4,-48(s0)
  6a:	97ba                	add	a5,a5,a4
  6c:	639c                	ld	a5,0(a5)
  6e:	853e                	mv	a0,a5
  70:	f91ff0ef          	jal	0 <put_str>
        if (i < argc - 1)
  74:	fdc42783          	lw	a5,-36(s0)
  78:	37fd                	addiw	a5,a5,-1
  7a:	0007871b          	sext.w	a4,a5
  7e:	fec42783          	lw	a5,-20(s0)
  82:	2781                	sext.w	a5,a5
  84:	00e7d663          	bge	a5,a4,90 <main+0x4a>
        {
            put_str(" ");
  88:	0f000513          	li	a0,240
  8c:	f75ff0ef          	jal	0 <put_str>
    for (int i = 1; i < argc; i++)
  90:	fec42783          	lw	a5,-20(s0)
  94:	2785                	addiw	a5,a5,1
  96:	fef42623          	sw	a5,-20(s0)
  9a:	fec42783          	lw	a5,-20(s0)
  9e:	873e                	mv	a4,a5
  a0:	fdc42783          	lw	a5,-36(s0)
  a4:	2701                	sext.w	a4,a4
  a6:	2781                	sext.w	a5,a5
  a8:	faf74ce3          	blt	a4,a5,60 <main+0x1a>
        }
    }
    put_str("\n");
  ac:	0f800513          	li	a0,248
  b0:	f51ff0ef          	jal	0 <put_str>

    register uint64 a0_asm __asm__("a0") = 0; // status = 0
  b4:	4501                	li	a0,0
    register uint64 a7_asm __asm__("a7") = SYS_exit;
  b6:	4889                	li	a7,2
    __asm__ volatile("ecall" : : "r"(a0_asm), "r"(a7_asm) : "memory");
  b8:	00000073          	ecall
    return 0;
  bc:	4781                	li	a5,0
}
  be:	853e                	mv	a0,a5
  c0:	70a2                	ld	ra,40(sp)
  c2:	7402                	ld	s0,32(sp)
  c4:	6145                	addi	sp,sp,48
  c6:	8082                	ret

00000000000000c8 <getpid>:
#define SYS_write 16
#define SYS_read 17
#define SYS_close 18
    .global getpid
getpid:
    li      a7, SYS_getpid
  c8:	48ad                	li	a7,11
    ecall
  ca:	00000073          	ecall
    ret
  ce:	8082                	ret

00000000000000d0 <exit>:

    .global exit
exit:
    li      a7, SYS_exit
  d0:	4889                	li	a7,2
    ecall
  d2:	00000073          	ecall
    ret
  d6:	8082                	ret

00000000000000d8 <write>:

    .global write
write:
    li      a7, SYS_write
  d8:	48c1                	li	a7,16
    ecall
  da:	00000073          	ecall
    ret
  de:	8082                	ret

00000000000000e0 <fork>:

    .global fork
fork:
    li      a7, SYS_fork
  e0:	4885                	li	a7,1
    ecall
  e2:	00000073          	ecall
    ret
  e6:	8082                	ret

00000000000000e8 <wait>:

    .global wait
wait:
    li      a7, SYS_wait
  e8:	488d                	li	a7,3
    ecall
  ea:	00000073          	ecall
    ret
  ee:	8082                	ret
