# initcode.S
# 目标：实现 getpid -> fork -> wait -> exit 的完整闭环

    .section .text.entry
    .global  _start

_start:
# ==========================================
# 1. 打印进程号 (getpid)
# ==========================================
    li       a7, 11                 # SYS_getpid
    ecall
    mv       s0, a0                 # 暂存 PID 到 s0

# 打印 "PID: "
    li       a0, 1                  # fd = 1
    jal      ra, msg_pid_addr
.ascii "PID:
    "
    .align   2
msg_pid_addr:
    mv       a1, ra
    li       a2, 5
    li       a7, 16                 # SYS_write
    ecall

# 打印数字 PID (将 s0 转为 ASCII)
    addi     s1, s0, 48             # '0' = 48
    addi     sp, sp, -16            # 开辟临时栈空间
    sb       s1, 0(sp)
    li       a0, 1                  # fd = 1
    mv       a1, sp
    li       a2, 1
    li       a7, 16                 # SYS_write
    ecall

# 打印换行
    li       t0, 10                 # '\n'
    sb       t0, 0(sp)
    li       a0, 1
    mv       a1, sp
    li       a2, 1
    li       a7, 16
    ecall
    addi     sp, sp, 16             # 恢复栈

# ==========================================
# 2. fork 测试
# ==========================================
    li       a7, 1                  # SYS_fork
    ecall

# 此时 a0 在父进程中是子进程 PID，在子进程中是 0
    beqz     a0, child_label

parent_label:
# ==========================================
# 3. 父进程分支: wait(&status)
# ==========================================
# 在栈上预留 8 字节给 status，并将地址传给 wait
    addi     sp, sp, -16
    mv       a0, sp                 # a0 = &status (栈地址)
    li       a7, 3                  # SYS_wait
    ecall
# 回收成功后，a0 为子进程 PID

# 打印 "Parent: Child recovered\n"
    li       a0, 1
    jal      ra, msg_parent_addr
.ascii "Parent:
    Child    recovered\n"
    .align   2
msg_parent_addr:
    mv       a1, ra
    li       a2, 24
    li       a7, 16
    ecall

# 正常退出 exit(0)
    li       a0, 0
    li       a7, 2
    ecall

child_label:
# ==========================================
# 4. 子进程分支: exit(1)
# ==========================================
# 打印 "I am child process\n"
    li       a0, 1
    jal      ra, msg_child_addr
    .ascii   "I am child process\n"
    .align   2
msg_child_addr:
    mv       a1, ra
    li       a2, 19
    li       a7, 16
    ecall

# 异常退出 exit(1)
    li       a0, 1
    li       a7, 2
    ecall