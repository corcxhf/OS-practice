#include "myos-cxx-sys.h"
#include <new>

extern "C" void __cxa_pure_virtual()
{
    myos_write_str("CXX_RUNTIME_FAIL pure-virtual\n");
    myos_exit(1);
}

extern "C" int __cxa_atexit(void (*)(void *), void *, void *)
{
    return 0;
}

extern "C"
{
    void *__dso_handle = (void *)&__dso_handle;
}

namespace
{
using block_align = long;

union block_header
{
    struct
    {
        block_header *next;
        unsigned long units;
    } s;
    block_align align;
};

block_header base;
block_header *free_list;

void cxx_free(void *ptr);

block_header *morecore(unsigned long units)
{
    if (units < 1024)
        units = 1024;

    myos_uint64 raw = myos_syscall3(MYOS_SYS_sbrk, units * sizeof(block_header), 0, 0);
    if ((long)raw < 0)
        return nullptr;

    block_header *block = reinterpret_cast<block_header *>(raw);
    block->s.units = units;
    cxx_free(static_cast<void *>(block + 1));
    return free_list;
}

void cxx_free(void *ptr)
{
    block_header *block;
    block_header *p;

    if (!ptr)
        return;

    if (!free_list)
    {
        base.s.next = free_list = &base;
        base.s.units = 0;
    }

    block = static_cast<block_header *>(ptr) - 1;
    for (p = free_list; !(block > p && block < p->s.next); p = p->s.next)
    {
        if (p >= p->s.next && (block > p || block < p->s.next))
            break;
    }

    if (block + block->s.units == p->s.next)
    {
        block->s.units += p->s.next->s.units;
        block->s.next = p->s.next->s.next;
    }
    else
    {
        block->s.next = p->s.next;
    }

    if (p + p->s.units == block)
    {
        p->s.units += block->s.units;
        p->s.next = block->s.next;
    }
    else
    {
        p->s.next = block;
    }
    free_list = p;
}

void *cxx_malloc(unsigned long size)
{
    block_header *p;
    block_header *prev;
    unsigned long units;

    if (size == 0)
        size = 1;

    units = (size + sizeof(block_header) - 1) / sizeof(block_header) + 1;
    if (!free_list)
    {
        base.s.next = free_list = prev = &base;
        base.s.units = 0;
    }
    else
    {
        prev = free_list;
    }

    for (p = prev->s.next;; prev = p, p = p->s.next)
    {
        if (p->s.units >= units)
        {
            if (p->s.units == units)
            {
                prev->s.next = p->s.next;
            }
            else
            {
                p->s.units -= units;
                p += p->s.units;
                p->s.units = units;
            }
            free_list = prev;
            return static_cast<void *>(p + 1);
        }

        if (p == free_list)
        {
            p = morecore(units);
            if (!p)
                return nullptr;
        }
    }
}
}

void *operator new(unsigned long size)
{
    void *ptr = cxx_malloc(size);
    if (!ptr)
    {
        myos_write_str("CXX_RUNTIME_FAIL new\n");
        myos_exit(1);
    }
    return ptr;
}

void *operator new[](unsigned long size)
{
    return operator new(size);
}

void *operator new(unsigned long size, const std::nothrow_t &) noexcept
{
    return cxx_malloc(size);
}

void *operator new[](unsigned long size, const std::nothrow_t &) noexcept
{
    return cxx_malloc(size);
}

void operator delete(void *ptr) noexcept
{
    cxx_free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    cxx_free(ptr);
}

void operator delete(void *ptr, unsigned long) noexcept
{
    cxx_free(ptr);
}

void operator delete[](void *ptr, unsigned long) noexcept
{
    cxx_free(ptr);
}

void operator delete(void *ptr, const std::nothrow_t &) noexcept
{
    cxx_free(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t &) noexcept
{
    cxx_free(ptr);
}
