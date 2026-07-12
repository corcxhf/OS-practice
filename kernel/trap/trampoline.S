    .section   .text
    .global    uservec
    .align     4                # stvec 寄存器要求地址必须是 4 字节对齐

uservec:
# 1. 交换 a0 和 sscratch (a0 指向 trapframe)
    csrrw      a0, sscratch, a0

# ---------------------------------------------------
# 【新增这一步】：保存发生陷阱时的 PC
# ---------------------------------------------------
    csrr       t0, sepc         # 从硬件读取真正出事的地址
    sd         t0, 24(a0)       # 存入 trapframe->epc (偏移 24)

    sd         ra, 40(a0)
    sd         sp, 48(a0)
    sd         gp, 56(a0)
    sd         tp, 64(a0)
    sd         t0, 72(a0)
    sd         t1, 80(a0)
    sd         t2, 88(a0)
    sd         s0, 96(a0)
    sd         s1, 104(a0)
    sd         a1, 120(a0)
    sd         a2, 128(a0)
    sd         a3, 136(a0)
    sd         a4, 144(a0)
    sd         a5, 152(a0)
    sd         a6, 160(a0)
    sd         a7, 168(a0)
    sd         s2, 176(a0)
    sd         s3, 184(a0)
    sd         s4, 192(a0)
    sd         s5, 200(a0)
    sd         s6, 208(a0)
    sd         s7, 216(a0)
    sd         s8, 224(a0)
    sd         s9, 232(a0)
    sd         s10, 240(a0)
    sd         s11, 248(a0)
    sd         t3, 256(a0)
    sd         t4, 264(a0)
    sd         t5, 272(a0)
    sd         t6, 280(a0)

    fsd        f0, 288(a0)
    fsd        f1, 296(a0)
    fsd        f2, 304(a0)
    fsd        f3, 312(a0)
    fsd        f4, 320(a0)
    fsd        f5, 328(a0)
    fsd        f6, 336(a0)
    fsd        f7, 344(a0)
    fsd        f8, 352(a0)
    fsd        f9, 360(a0)
    fsd        f10, 368(a0)
    fsd        f11, 376(a0)
    fsd        f12, 384(a0)
    fsd        f13, 392(a0)
    fsd        f14, 400(a0)
    fsd        f15, 408(a0)
    fsd        f16, 416(a0)
    fsd        f17, 424(a0)
    fsd        f18, 432(a0)
    fsd        f19, 440(a0)
    fsd        f20, 448(a0)
    fsd        f21, 456(a0)
    fsd        f22, 464(a0)
    fsd        f23, 472(a0)
    fsd        f24, 480(a0)
    fsd        f25, 488(a0)
    fsd        f26, 496(a0)
    fsd        f27, 504(a0)
    fsd        f28, 512(a0)
    fsd        f29, 520(a0)
    fsd        f30, 528(a0)
    fsd        f31, 536(a0)
    frcsr      t0
    sd         t0, 544(a0)


    csrr       t0, sscratch
    sd         t0, 112(a0)      # a0 的偏移量


    ld         sp, 8(a0)
    ld         t1, 0(a0)
    csrw       satp, t1
    sfence.vma zero, zero
    ld         t0, 16(a0)
    jr         t0               # 直接跳过去！
