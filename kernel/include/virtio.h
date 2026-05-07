// virtio.h
// VirtIO 设备的硬件寄存器定义和数据结构
// 适用于 xv6-riscv 和 QEMU 虚拟环境

#ifndef VIRTIO_H
#define VIRTIO_H

// VirtIO MMIO 控制寄存器基地址 (通常在 QEMU 的 virt 机器模型中是这个地址)
// 如果你的 memlayout.h 里已经定义了 VIRTIO0，这一行可以注释掉
#ifndef VIRTIO0
#define VIRTIO0 0x10001000
#endif

// 辅助宏：用于读写 VirtIO 寄存器
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

// VirtIO MMIO 控制寄存器偏移量 (从 qemu virtio_mmio.h 中提取)
#define VIRTIO_MMIO_MAGIC_VALUE 0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION 0x004     // 版本号
#define VIRTIO_MMIO_DEVICE_ID 0x008   // 设备 ID; 2 表示块设备 (disk)
#define VIRTIO_MMIO_VENDOR_ID 0x00c   // 供应商 ID; 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028  // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL 0x030        // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034    // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM 0x038        // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN 0x03c      // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN 0x040        // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY 0x044      // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050     // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064    // write-only
#define VIRTIO_MMIO_STATUS 0x070           // read/write

// VirtIO v1 标准的新 MMIO 寄存器
#define VIRTIO_MMIO_QUEUE_DESC_LOW 0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW 0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH 0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW 0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH 0x0a4

// 状态寄存器位 (从 qemu virtio_config.h 提取)
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER 2
#define VIRTIO_CONFIG_S_DRIVER_OK 4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// 设备特性位
#define VIRTIO_BLK_F_RO 5          /* 磁盘只读 */
#define VIRTIO_BLK_F_SCSI 7        /* 支持 SCSI 穿透 */
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* 支持写回模式 */
#define VIRTIO_BLK_F_MQ 12         /* 支持多个虚拟队列 */
#define VIRTIO_F_ANY_LAYOUT 27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29

// VirtIO 描述符数量，必须是 2 的幂
#define NUM 8

// --- VirtQueue 数据结构定义 ---

// 一个单独的描述符 (表示内存中的一块区域)
struct virtq_desc
{
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};
#define VRING_DESC_F_NEXT 1  // 缓冲链表继续
#define VRING_DESC_F_WRITE 2 // 驱动程序给设备写数据的缓冲区

// 驱动向设备提供的可用描述符环
struct virtq_avail
{
    uint16 flags;
    uint16 idx;
    uint16 ring[NUM];
    uint16 unused;
};

// 设备向驱动返回的已使用描述符记录
struct virtq_used_elem
{
    uint32 id; // 我们写入 avail ring 的那个起始描述符的 index
    uint32 len;
};

// 已使用描述符环
struct virtq_used
{
    uint16 flags;
    uint16 idx;
    struct virtq_used_elem ring[NUM];
};

// 块设备（磁盘）特定的请求结构
#define VIRTIO_BLK_T_IN 0  // 读磁盘
#define VIRTIO_BLK_T_OUT 1 // 写磁盘

// 磁盘请求头结构体
struct virtio_blk_req
{
    uint32 type; // VIRTIO_BLK_T_IN 还是 VIRTIO_BLK_T_OUT
    uint32 reserved;
    uint64 sector; // 起始扇区号
};

#endif // VIRTIO_H