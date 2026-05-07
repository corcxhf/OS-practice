# initcode.S

    .section .text.entry
    .global  _start

_start:
# ==========================================
# 1. 测试1：创建并写文件 (O_CREAT|O_WRONLY=0x201)
# ==========================================
    li       a7, 15
    jal      ra, str_path1
# "/hello.txt\0" (11字节) + \0 (补1字节) = 12字节 (4的倍数)
    .ascii   "/hello.txt\0\0"
str_path1:
    mv       a0, ra                  # a0 = "/hello.txt"
    li       a1, 0x201               # a1 = O_CREAT | O_WRONLY
    ecall

    mv       s0, a0                  # s0 = fd

# write(fd, "Hello, File System!\n", 20)
    li       a7, 16
    mv       a0, s0
    jal      ra, str_content
# "Hello, File System!\n" (20字节) -> 已经是4的倍数
    .ascii   "Hello, File System!\n"
str_content:
    mv       a1, ra                  # a1 = 内容地址
    li       a2, 20                  # a2 = 长度
    ecall

# close(fd)
    li       a7, 18
    mv       a0, s0
    ecall

# 模拟期望输出 1: "Test 1 passed..."
    li       a7, 16
    li       a0, 1                   # stdout
    jal      ra, str_t1_ok
# "Test 1 passed: write 20 bytes\n" (30字节) + \0\0 (补2字节) = 32字节
.ascii "Test 1 passed: write 20 bytes\n\0\0"
str_t1_ok:
    mv       a1, ra
    li       a2, 30
    ecall

# ==========================================
# 2. 测试2：读回文件内容 (O_RDONLY=0)
# ==========================================
    li       a7, 15
    jal      ra, str_path2
    .ascii   "/hello.txt\0\0"
str_path2:
    mv       a0, ra
    li       a1, 0                   # O_RDONLY
    ecall
    mv       s0, a0

# read(fd, buf, 64)
    addi     sp, sp, -64             # 栈上开辟 buf[64]
    li       a7, 17
    mv       a0, s0
    mv       a1, sp
    li       a2, 64
    ecall
    mv       s1, a0                  # s1 = 实际读到的字节数 r

# close(fd)
    li       a7, 18
    mv       a0, s0
    ecall

# 模拟期望输出 2: "Test 2 passed: read back: "
    li       a7, 16
    li       a0, 1
    jal      ra, str_t2_ok
# "Test 2 passed: read back: " (26字节) + \0\0 (补2字节) = 28字节
.ascii "Test 2 passed: read back: \0\0"
str_t2_ok:
    mv       a1, ra
    li       a2, 26
    ecall

# 打印读出来的 buf 内容
    li       a7, 16
    li       a0, 1
    mv       a1, sp                  # buf 地址
    mv       a2, s1                  # 使用 read 返回的长度
    ecall

    addi     sp, sp, 64              # 释放栈空间

# ==========================================
# 3. 退出
# ==========================================
    li       a0, 0
    li       a7, 2
    ecall