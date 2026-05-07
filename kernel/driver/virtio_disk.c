// virtio_disk.c
// QEMU 虚拟磁盘驱动 (VirtIO block device)

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "param.h"
#include "memlayout.h"
#include "fs.h"
#include "virtio.h"
extern void *memset(void *dst, int v, unsigned long n);

extern void acquire(struct spinlock *);
extern void initlock(struct spinlock *lk, char *name);
extern void release(struct spinlock *lk);

extern int holding(struct spinlock *lk);

void virtio_disk_intr();

struct disk
{
    // 保护磁盘请求队列的自旋锁
    struct spinlock vdisk_lock;

    // VirtIO 描述符环
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;

    // 记录每个描述符的状态
    char free[NUM];  // 如果描述符可用则为 1
    uint16 used_idx; // 我们已经处理到 used ring 的哪个位置

    // 记录每个正在执行的请求
    struct
    {
        struct buf *b;
        char status;
    } info[NUM];
};

static struct disk disk;
// ==========================================
// 请将这行定义放在 virtio_disk_init 函数的外面（比如 disk 结构体定义的下方）
// 这保证了我们有一块连续的、按 4096 对齐的 8KB 物理内存
static char vring_mem[2 * PGSIZE] __attribute__((aligned(PGSIZE)));
// ==========================================

void virtio_disk_init(void)
{
    uint32 status = 0;

    initlock(&disk.vdisk_lock, "virtio_disk");

    // 1. 检查魔法数字和版本
    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 1 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
    {
        panic("could not find virtio disk");
    }

    // 2. 握手与确认
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 3. 协商特性
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 注意：【删除了这里的 DRIVER_OK，移到了最后！】

    *R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

    // 4. 选择和检查队列
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM)
        panic("virtio disk max queue too short");

    // 5. 【核心修复】：在连续的内存上划分 desc, avail 和 used
    memset(vring_mem, 0, sizeof(vring_mem));

    // desc 放在开头
    disk.desc = (struct virtq_desc *)vring_mem;

    // avail 紧跟在 desc 后面
    disk.avail = (struct virtq_avail *)(vring_mem + NUM * sizeof(struct virtq_desc));

    // used 必须放在下一个 PGSIZE 边界上 (这是硬件通过 ALIGN=PGSIZE 算出来的位置)
    disk.used = (struct virtq_used *)(vring_mem + PGSIZE);

    // 6. 设置队列大小
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

    // 7. 设置对齐，并写入 PFN 激活队列
    *R(VIRTIO_MMIO_QUEUE_ALIGN) = PGSIZE;
    *R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.desc) >> 12;

    // 8. 初始化空闲描述符表
    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    // 9. 【终极一步】：所有配置完毕，告诉硬件可以发车了！
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
}
// 分配三个描述符
static int
alloc3_desc(int *idx)
{
    for (int i = 0; i < 3; i++)
    {
        int match = 0;
        for (int j = 0; j < NUM; j++)
        {
            if (disk.free[j])
            {
                disk.free[j] = 0;
                idx[i] = j;
                match = 1;
                break;
            }
        }
        if (!match)
        {
            // 如果分配失败，把之前分配的退回去
            for (int j = 0; j < i; j++)
                disk.free[idx[j]] = 1;
            return -1;
        }
    }
    return 0;
}

// 释放描述符
static void
free_desc(int i)
{
    if (i >= NUM)
        panic("free_desc 1");
    if (disk.free[i])
        panic("free_desc 2");
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    wakeup(&disk.free[0]);
}

static void
free_chain(int i)
{
    while (1)
    {
        int flag = disk.desc[i].flags;
        int nxt = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

// 核心读写函数（你在 bio.c 中调用的就是这个）
void virtio_disk_rw(struct buf *b, int write)
{
    uint64 sector = b->blockno * (BSIZE / 512);
    acquire(&disk.vdisk_lock);

    int idx[3];
    while (1)
    {
        if (alloc3_desc(idx) == 0)
        {
            break;
        }
        sleep(&disk.free[0], &disk.vdisk_lock);
    }
    static struct virtio_blk_req disk_req[NUM];
    struct virtio_blk_req *req = &disk_req[idx[0]];

    req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;

    disk.desc[idx[0]].addr = (uint64)req;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    disk.desc[idx[1]].addr = (uint64)b->data;
    disk.desc[idx[1]].len = BSIZE;
    disk.desc[idx[1]].flags = write ? 0 : VRING_DESC_F_WRITE;
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    disk.info[idx[0]].status = 0;
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next = 0;

    b->disk = 1;
    disk.info[idx[0]].b = b;

    disk.avail->ring[disk.avail->idx % NUM] = idx[0];
    __sync_synchronize();
    disk.avail->idx += 1;
    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // 发送通知

    struct proc *p = myproc();

    while (b->disk == 1)
    {
        if (p != 0)
            sleep(b, &disk.vdisk_lock);

        else
        {
            release(&disk.vdisk_lock);
            virtio_disk_intr();
            acquire(&disk.vdisk_lock);
        }
    }

    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    release(&disk.vdisk_lock);
}

// 磁盘中断处理
void virtio_disk_intr()
{
    acquire(&disk.vdisk_lock);

    // 确认中断
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
    __sync_synchronize();

    // 检查已完成的请求
    while (disk.used_idx != disk.used->idx)
    {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;

        if (disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        struct buf *b = disk.info[id].b;
        __sync_synchronize();
        b->disk = 0; // 标记完成

        // printf("Content in the disk:      \n");
        // for (int i = 0; i < 64; i++)
        //     printf("%d", b->data[i]);

        wakeup(b); // 唤醒 bio.c 中正在 sleep 的进程

        disk.used_idx += 1;
    }

    release(&disk.vdisk_lock);
}