    /*     swtch.S — 内核上下文切换汇编（Lab5 任务2）
    *
    *      函数签名：void swtch(struct context *old, struct context *new);
    *
    *      调用约定（RISC-V ABI）：
    *      a0 = old（指向要保存当前寄存器的 context 结构）
    *      a1 = new（指向要恢复的目标 context 结构）
    *
    *      这个函数做了一件神奇的事：
    *      - 调用前：CPU 在"旧"进程（或调度器）的代码里运行
    *      - 函数返回后：CPU 神不知鬼不觉地在"新"进程（或调度器）的代码里运行了！
    *      - 秘密：ret 指令会把 PC 设置为 ra 寄存器的值，
    *      而我们在恢复 ra = new->ra，所以 ret 后会跳到新进程设定的位置。
    *
    *      注意：swtch 只保存 Callee-Saved 寄存器！
    *      因为 swtch() 是被 C 代码调用的普通函数，
    *      Caller-Saved 寄存器（t0~t6, a0~a7）已在函数调用前被 C 编译器自动处理。
    */

    .globl swtch
swtch:
    sd     s0, 0(a0)
    sd     s1, 8(a0)
    sd     s2, 16(a0)
    sd     s3, 24(a0)
    sd     s4, 32(a0)
    sd     s5, 40(a0)
    sd     s6, 48(a0)
    sd     s7, 56(a0)
    sd     s8, 64(a0)
    sd     s9, 72(a0)
    sd     s10, 80(a0)
    sd     s11, 88(a0)
    sd     ra, 96(a0)
    sd     sp, 104(a0)

    ld     s0, 0(a1)
    ld     s1, 8(a1)
    ld     s2, 16(a1)
    ld     s3, 24(a1)
    ld     s4, 32(a1)
    ld     s5, 40(a1)
    ld     s6, 48(a1)
    ld     s7, 56(a1)
    ld     s8, 64(a1)
    ld     s9, 72(a1)
    ld     s10, 80(a1)
    ld     s11, 88(a1)
    ld     ra, 96(a1)
    ld     sp, 104(a1)
    
    ret
