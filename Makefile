# Makefile — 构建系统
# ============================================================
# 工具链配置
# ============================================================
CROSS   = riscv64-unknown-elf-
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
OBJCOPY = $(CROSS)objcopy

# ============================================================
# 编译标志
# ============================================================
CFLAGS = -nostdlib -fno-builtin -mcmodel=medany \
         -march=rv64gc -mabi=lp64d \
         -g -Wall -ffreestanding \
         -I kernel/include

# ============================================================
# 源文件路径定义
# ============================================================
U = user
K = kernel

SRCS = \
    $K/boot/entry.S \
    $K/driver/uart.c \
    $K/boot/main.c \
    $K/boot/start.c \
    $K/driver/console.c \
    $K/mm/kalloc.c \
    $K/mm/vm.c \
    $K/trap/kernelvec.S \
    $K/trap/trap.c \
    $K/trap/timervec.S \
    $K/trap/plic.c \
    $K/proc/proc.c \
    $K/proc/swtch.S \
    $K/trap/trampoline.S \
    $K/syscall/syscall.c \
    $K/syscall/sysproc.c \
    $K/proc/spinlock.c \
    $K/trap/userret.S\
    $K/fs/bio.c\
    $K/fs/file.c\
    $K/fs/fs.c\
    $K/syscall/sysfile.c\
    $K/driver/virtio_disk.c


KERNEL   = kernel.elf
LDSCRIPT = kernel.ld


all: $(KERNEL)

mkfs: mkfs.c
	gcc mkfs.c -o mkfs

fs.img: mkfs
	@echo "正在创建并格式化磁盘镜像 fs.img..."
	./mkfs

$U/initcode.out: $U/initcode.S $U/initcode.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -fno-pic -fno-pie -T $U/initcode.ld -o $U/initcode.out $U/initcode.S

$U/initcode.bin: $U/initcode.out
	$(OBJCOPY) -S -O binary $U/initcode.out $U/initcode.bin

$U/initcode.o: $U/initcode.bin
	# 进入 user 目录执行链接，这样符号名就不会带 user_ 前缀
	cd $U && $(LD) -r -b binary -o initcode.o initcode.bin

$(KERNEL): $(SRCS) $(LDSCRIPT) $U/initcode.o
	$(CC) $(CFLAGS) -T $(LDSCRIPT) $(SRCS) $U/initcode.o -o $@
	@echo "======================================"
	@echo " 内核编译成功：$(KERNEL)"
	@echo "======================================"

QEMU_DISK = -drive file=fs.img,if=none,format=raw,id=x0 \
            -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

run: $(KERNEL) fs.img
	qemu-system-riscv64 \
	-machine virt \
	-bios none \
	-kernel $(KERNEL) \
	$(QEMU_DISK) \
	-nographic

debug: $(KERNEL) fs.img
	-pkill -f qemu-system-riscv64
	qemu-system-riscv64 \
	-machine virt \
	-bios none \
	-kernel $(KERNEL) \
	$(QEMU_DISK) \
	-nographic \
	-s -S 

clean: 
	rm -f $(KERNEL) *.o *.d $U/*.o $U/*.out $U/*.bin $U/*.d mkfs fs.img

.PHONY: all run debug clean