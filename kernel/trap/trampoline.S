    .section .text
    .global  uservec
    .align   4                # stvec 寄存器要求地址必须是 4 字节对齐

uservec:
# 1. 交换 a0 和 sscratch (a0 指向 trapframe)
    csrrw    a0, sscratch, a0

# ---------------------------------------------------
# 【新增这一步】：保存发生陷阱时的 PC
# ---------------------------------------------------
    csrr     t0, sepc         # 从硬件读取真正出事的地址
    sd       t0, 24(a0)       # 存入 trapframe->epc (偏移 24)

    sd       ra, 40(a0)
    sd       sp, 48(a0)
    sd       gp, 56(a0)
    sd       tp, 64(a0)
    sd       t0, 72(a0)
    sd       t1, 80(a0)
    sd       t2, 88(a0)
    sd       s0, 96(a0)
    sd       s1, 104(a0)
    sd       a1, 120(a0)
    sd       a2, 128(a0)
    sd       a3, 136(a0)
    sd       a4, 144(a0)
    sd       a5, 152(a0)
    sd       a6, 160(a0)
    sd       a7, 168(a0)
    sd       s2, 176(a0)
    sd       s3, 184(a0)
    sd       s4, 192(a0)
    sd       s5, 200(a0)
    sd       s6, 208(a0)
    sd       s7, 216(a0)
    sd       s8, 224(a0)
    sd       s9, 232(a0)
    sd       s10, 240(a0)
    sd       s11, 248(a0)
    sd       t3, 256(a0)
    sd       t4, 264(a0)
    sd       t5, 272(a0)
    sd       t6, 280(a0)

# 3. 拯救用户的 a0
# 将用户的 a0 从 sscratch 读到 t0 中，然后保存到 trapframe
    csrr     t0, sscratch
    sd       t0, 112(a0)      # a0 的偏移量

# 4. 恢复内核环境
# 从 trapframe 读取内核栈顶地址，并赋给真正的 sp 寄存器
# 假设你的 kernel_sp 存放在 trapframe 的偏移 8 处
    ld       sp, 8(a0)

# 5. 跳转到 C 语言的 usertrap 处理函数
# 假设 kernel_trap (即 usertrap的地址) 存放在偏移 16 处
    ld       t0, 16(a0)
    jr       t0               # 直接跳过去！