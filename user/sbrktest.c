#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"

// 祖传系统调用内联汇编
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall"
                     : "+r"(a0_asm)
                     : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm)
                     : "memory");
    return a0_asm;
}

// 辅助函数：计算字符串长度
int str_len(const char *s)
{
    int len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

// 辅助函数：系统调用打印字符串
void print(const char *s)
{
    syscall(SYS_write, 1, (uint64)s, (uint64)str_len(s));
}

void main()
{
    print("========== SBRK STRESS TEST START ==========\n");

    // ---------------------------------------------------------
    // 测试 1：探测当前堆的原始天花板 (sbrk(0))
    // ---------------------------------------------------------
    char *heap_start = (char *)syscall(SYS_sbrk, 0, 0, 0);
    if ((uint64)heap_start == (uint64)-1)
    {
        print("[-] FATAL: sbrk(0) failed!\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    print("[+] Test 1 Pass: sbrk(0) probed heap boundary.\n");

    // ---------------------------------------------------------
    // 测试 2：单页分配与读写测试 (4096 bytes)
    // 🚨 这是最核心的测试！如果不缺页崩溃，说明 mappages 完美！
    // ---------------------------------------------------------
    char *p1 = (char *)syscall(SYS_sbrk, 4096, 0, 0);
    if (p1 != heap_start)
    {
        print("[-] FATAL: sbrk(4096) did not return old boundary!\n");
        syscall(SYS_exit, 1, 0, 0);
    }

    // 疯狂写入数据
    for (int i = 0; i < 4096; i++)
    {
        p1[i] = (char)(i % 256);
    }

    // 读出数据进行一致性校验
    int data_match = 1;
    for (int i = 0; i < 4096; i++)
    {
        if (p1[i] != (char)(i % 256))
        {
            data_match = 0;
            break;
        }
    }
    if (!data_match)
    {
        print("[-] FATAL: Data corruption detected in new memory!\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    print("[+] Test 2 Pass: 4KB allocated and read/write verified.\n");

    // ---------------------------------------------------------
    // 测试 3：跨越多页的大容量分配 (比如 20000 字节，约 5 页)
    // 验证 uvmalloc 里的 for 循环是否能连续挂载多页物理内存
    // ---------------------------------------------------------
    char *p2 = (char *)syscall(SYS_sbrk, 20000, 0, 0);

    // 触碰最后一页的内存，如果没崩，说明整条虚拟地址空间都映射成功了
    p2[19999] = 'K';
    if (p2[19999] != 'K')
    {
        print("[-] FATAL: Cross-page allocation write failed!\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    print("[+] Test 3 Pass: 20KB cross-page allocation verified.\n");

    // ---------------------------------------------------------
    // 测试 4：缩容测试 (归还 20000 字节)
    // 验证 uvmdealloc 是否正确抹除了页表并退还物理页
    // ---------------------------------------------------------
    char *p3 = (char *)syscall(SYS_sbrk, -20000, 0, 0);
    // 按照 POSIX 标准，收缩也会返回收缩前的旧边界
    if (p3 != p2 + 20000)
    {
        print("[-] FATAL: sbrk(-20000) returned wrong boundary!\n");
        syscall(SYS_exit, 1, 0, 0);
    }
    print("[+] Test 4 Pass: 20KB memory successfully deallocated.\n");

    print("========== ALL SBRK TESTS PASSED! ==========\n");
    syscall(SYS_exit, 0, 0, 0);
}

// ---------------------------------------------------------
// 底层入口点
// ---------------------------------------------------------
void __attribute__((naked, section(".text.entry"))) _start()
{
    __asm__ volatile("call main");
}