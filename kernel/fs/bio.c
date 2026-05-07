/* bio.c — 块设备缓冲层（Lab7 任务1）
 *
 * 原则：所有文件系统的磁盘读写，都必须经过这层缓冲！
 *
 * 数据结构：
 *   struct buf — 内存中缓存一个磁盘块（512字节或1024字节）的结构体
 *   bcache     — 所有 buf 的缓冲池，形成双向 LRU 链表
 *
 * 核心操作：
 *   bread(dev, blockno) — 读取磁盘块（优先从缓存取，没有则从磁盘读）
 *   bwrite(b)           — 将修改过的缓冲块写回磁盘
 *   brelse(b)           — 释放缓冲块的占用锁（归还给缓冲池）
 *
 * 关键性能优化：LRU（最近最少使用）策略淘汰缓存块
 */

#include "defs.h"
#include "memlayout.h"
#include "param.h"
#include "riscv.h"
#include "types.h"
#include "fs.h"

/* 磁盘块大小（字节，与文件系统层约定一致）*/
#define BSIZE 1024
#define BPB (BSIZE * 8)                              /* 每个块包含的位数 (1024*8 = 8192) */
#define BMAP_BLOCK(b, sb) ((b) / BPB + sb.bmapstart) /* 块号 b 对应的位图块号 */

struct superblock sb;
/* 缓冲块结构（已提供，无需修改）*/

/* 缓冲池（全局，内核中只有一个）*/
struct
{
  struct buf buf[NBUF]; /* 固定大小的缓冲区数组（NBUF 在 param.h 定义）*/
  struct buf head;      /* LRU 链表的哨兵头节点 */
} bcache;

/* 外部磁盘驱动函数（在 virtio_disk.c 中实现，本框架中已提供）*/
extern void virtio_disk_rw(struct buf *b, int write);

extern void *memset(void *dst, int v, unsigned long n);

extern void bzero(uint dev, uint bno);
/* ================================================================
 * binit — 初始化缓冲池（内核启动时调用）
 *
 * 任务：将所有 buf 连接成双向循环链表，以 bcache.head 为哨兵。
 * ================================================================ */
void binit(void)
{
  struct buf *b;

  /* 初始化链表头（哨兵节点自指）*/
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  /* ================================================================
   * TODO [Lab7-任务1-步骤1]：
   *   将 bcache.buf[] 中的每个 buf 逐个插入到链表头部（头插法）。
   *   每次头插需要修改 4 个指针：新节点的 prev/next，以及相邻节点的 next/prev。
   * ================================================================ */
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    /* 在这里插入链表 */
    b->next = bcache.head.next;
    bcache.head.next->prev = b;
    bcache.head.next = b;
    b->prev = &bcache.head;
  }
}

/* ================================================================
 * bget — 在缓冲池中找到指定 (dev, blockno) 的缓冲块
 *
 * 算法：
 *   1. 正向遍历链表，若找到 dev 和 blockno 匹配的 buf，引用计数+1，返回
 *   2. 若未找到，反向遍历链表，找到 refcnt==0 的块（最近最少使用）
 *      将其重用为新的 (dev, blockno) 缓冲，返回
 *   3. 若全部 buf 都在被使用，panic（缓冲池耗尽）
 *
 * 注意：返回的 buf 尚未从磁盘加载数据（由 bread 判断并加载）
 * ================================================================ */
struct buf *bget(uint dev, uint blockno)
{
  struct buf *b;

  /* 步骤1：查找是否已缓存 */
  for (b = bcache.head.next; b != &bcache.head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      return b;
    }
  }

  /* 步骤2：LRU 淘汰——从链表尾部（最久未使用）向前找空闲块 */
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev, b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      return b;
      /* ================================================================
       * TODO [Lab7-任务1-步骤2]：
       *   重用这个空闲缓冲块：
       *   1. 更新 b->dev 和 b->blockno 为新目标 dev/blockno
       *   2. 将 b->valid 设为 0（旧数据已失效，需要重新从磁盘读）
       *   3. 将 b->refcnt 设为 1（开始被使用）
       *   4. 返回 b
       * ================================================================ */
      (void)dev;
      (void)blockno;
    }
  }
  panic("bget: no buffers");
}

/* ================================================================
 * bread — 读取磁盘块（返回含有效数据的缓冲块）
 * ================================================================ */
struct buf *bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);

  if (!b->valid)
  {
    b->valid = 1;
    virtio_disk_rw(b, 0);
    /* 缓存未命中，需要从磁盘真正读取数据 */
    /* ================================================================
     * TODO [Lab7-任务1-步骤3]：
     *   调用磁盘驱动读取数据，并将 b->valid 标记为 1（数据已有效）。
     *   使用 virtio_disk_rw(b, 0)：第二个参数 0 表示读操作。
     * ================================================================ */
  }

  return b;
}

/* ================================================================
 * bwrite — 将缓冲块的修改写回磁盘
 * ================================================================ */
void bwrite(struct buf *b)
{
  virtio_disk_rw(b, 1);
  /* ================================================================
   * TODO [Lab7-任务1-步骤4]：
   *   调用磁盘驱动将缓冲数据写回磁盘。
   *   使用 virtio_disk_rw(b, 1)：第二个参数 1 表示写操作。
   * ================================================================ */
  (void)b;
}

/* ================================================================
 * brelse — 使用完毕，释放对缓冲块的占用
 *
 * 任务：
 *   1. 引用计数减 1
 *   2. 若引用计数归零（没人用了），把这个 buf 移动到 LRU 链表头部
 *      （表示"最近刚用完"，是最后淘汰的候选）
 * ================================================================ */
void brelse(struct buf *b)
{
  b->refcnt--;
  if (b->refcnt == 0)
  {
    b->prev->next = b->next;
    b->next->prev = b->prev;

    b->next = bcache.head.next;
    bcache.head.next->prev = b;
    b->prev = &bcache.head;
    bcache.head.next = b;
  }
  /* ================================================================
   * TODO [Lab7-任务1-步骤5]：
   *   1. b->refcnt--
   *   2. 若 b->refcnt == 0，将 b 从当前位置摘除，重新头插到链表头部：
   *      摘除操作需修改 4 个指针（b的前据的next、b的后继的prev）；
   *      头插到 bcache.head 后面同样需要 4 个指针修改。
   * ================================================================ */
  (void)b;
}
uint balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  /* 假设 sb 是通过读取超级块获取的结构体，包含文件系统元数据 */
  /* 这里我们遍历文件系统的所有块 */
  for (b = 0; b < sb.size; b += BPB)
  {
    /* 1. 读取包含块 b 位图信息的缓冲块 */
    bp = bread(dev, BMAP_BLOCK(b, sb));

    /* 2. 在当前位图块中按位查找 */
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++)
    {
      m = 1 << (bi % 8);

      /* 检查第 bi 位是否为 0 (空闲) */
      if ((bp->data[bi / 8] & m) == 0)
      {
        /* 找到空闲块！ */

        /* 3. 将该位置为 1 (标记为占用) */
        bp->data[bi / 8] |= m;

        /* 4. 将修改后的位图块同步回磁盘 */
        bwrite(bp);

        /* 5. 释放缓冲块锁 */
        brelse(bp);

        /* 6. [可选但推荐]：清空新分配的磁盘块内容，防止读到旧数据 */
        bzero(dev, b + bi);

        return b + bi; /* 返回分配的块号 */
      }
    }

    /* 当前位图块没找着，释放它，继续找下一个位图块 */
    brelse(bp);
  }

  panic("balloc: out of blocks");
  return 0;
}
void bzero(uint dev, uint bno)
{
  struct buf *bp;
  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  bwrite(bp);
  brelse(bp);
}

void bfree(uint dev, uint b)
{
  struct buf *bp;
  int bi, m;

  /* 1. 读取包含该块位图信息的缓冲块 */
  /* BMAP_BLOCK 计算公式与 balloc 一致 */
  bp = bread(dev, BMAP_BLOCK(b, sb));

  /* 2. 定位到位图块中的具体某一位 */
  bi = b % BPB;      /* 在当前位图块中的位偏移 */
  m = 1 << (bi % 8); /* 在对应字节中的掩码 */

  /* 3. 安全检查（可选）：如果该位本来就是 0，说明逻辑有误 */
  if ((bp->data[bi / 8] & m) == 0)
    panic("bfree: freeing free block");

  /* 4. 将该位清零 (1 -> 0) */
  bp->data[bi / 8] &= ~m;

  /* 5. 同步回磁盘并释放缓冲区 */
  bwrite(bp);
  brelse(bp);
}