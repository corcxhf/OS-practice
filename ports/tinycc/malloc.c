#include "riscv.h"
#include "proc.h"
#include "fs.h"
#include "types.h"


// 祖传系统调用
static inline uint64 syscall(uint64 n, uint64 a0, uint64 a1, uint64 a2)
{
    register uint64 a0_asm __asm__("a0") = a0;
    register uint64 a1_asm __asm__("a1") = a1;
    register uint64 a2_asm __asm__("a2") = a2;
    register uint64 a7_asm __asm__("a7") = n;
    __asm__ volatile("ecall" : "+r"(a0_asm) : "r"(a1_asm), "r"(a2_asm), "r"(a7_asm) : "memory");
    return a0_asm;
}

typedef long Align;

union header
{
    struct
    {
        union header *ptr; // 指向下一个空闲块
        uint size;         // 当前内存块的大小（以 sizeof(Header) 为单位）
    } s;
    Align x; // 强行占位实现对齐
};

typedef union header Header;

static Header base;       // 空闲链表的头部起点
static Header *freep = 0; // 指向当前空闲链表节点的指针

// 向内核申请更多物理页
static Header *morecore(uint nu)
{
    char *cp;
    Header *up;

    if (nu < 4096)
        nu = 4096; // 每次至少向内核要 4096 个区块，减少 sbrk 砸系统调用的频率

    // 调用你的无敌 SYS_sbrk
    cp = (char *)syscall(SYS_sbrk, nu * sizeof(Header), 0, 0);
    if (cp == (char *)-1)
        return 0; // 内核 OOM 了

    up = (Header *)cp;
    up->s.size = nu;

    // 把新申请的内存伪装成一块大空闲内存，调用 free 塞进链表里
    extern void free(void *);
    free((void *)(up + 1));
    return freep;
}

// 用户态标准 malloc
void *malloc(uint64 nbytes)
{
    Header *p, *prevp;
    uint nunits;

    // 计算实际需要多少个 Header 大小的区块 (包含控制头本身)
    nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;

    // 第一次调用，初始化空闲链表
    if ((prevp = freep) == 0)
    {
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }

    // 经典循环首次适应算法 (First Fit) 遍历空闲链表
    for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
    {
        if (p->s.size >= nunits)
        {
            if (p->s.size == nunits)
            {
                // 刚好大小相等，直接把这个节点从链表里摘除
                prevp->s.ptr = p->s.ptr;
            }
            else
            {
                // 块太大，从末尾切下一刀分给用户，剩下的留在链表里
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void *)(p + 1); // 返回跨过 Header 控制头之后的真实数据指针
        }
        if (p == freep)
        {
            // 链表转了一圈没找到够大的空位，呼叫内核扩容
            if ((p = morecore(nunits)) == 0)
                return 0;
        }
    }
}

// 用户态标准 free
void free(void *ap)
{
    Header *bp, *p;

    if (ap == 0)
        return;

    bp = (Header *)ap - 1; // 往前退一步，拿到控制头

    // 寻找合适的链表插入位置，保持链表地址有序（方便后面做内存碎片合并）
    for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    {
        if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    }

    // 尝试与后一个空闲块合并
    if (bp + bp->s.size == p->s.ptr)
    {
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    }
    else
    {
        bp->s.ptr = p->s.ptr;
    }

    // 尝试与前一个空闲块合并
    if (p + p->s.size == bp)
    {
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    }
    else
    {
        p->s.ptr = bp;
    }
    freep = p;
}

void *realloc(void *ap, uint64 nbytes)
{
    if (ap == 0)
        return malloc(nbytes);

    Header *bp = (Header *)ap - 1;
    uint old_bytes = (bp->s.size - 1) * sizeof(Header);

    if (old_bytes >= nbytes)
        return ap; // 原有空间本来就够大，原地不动

    // 空间不够，申请一块全新的
    void *new_ptr = malloc(nbytes);
    if (new_ptr == 0)
        return 0;

    // 搬运原数据
    char *src = (char *)ap;
    char *dst = (char *)new_ptr;
    for (uint i = 0; i < old_bytes; i++)
    {
        dst[i] = src[i];
    }

    // 释放老内存
    free(ap);
    return new_ptr;
}