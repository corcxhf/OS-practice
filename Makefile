# Makefile — 构建系统（Lab1 完成后即可使用）
#
# 使用方法：
#   make        # 编译内核
#   make run    # 编译并在 QEMU 中启动内核
#   make debug  # 启动 QEMU 并等待 GDB 连接（端口 1234）
#   make clean  # 清除所有编译产物

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
# 内核源文件列表（注意：绝不能包含用户态的 initcode.c ！）
# ============================================================
SRCS = \
    kernel/boot/entry.S \
    kernel/driver/uart.c \
    kernel/boot/main.c \
    kernel/boot/start.c \
    kernel/driver/console.c \
    kernel/mm/kalloc.c \
    kernel/mm/vm.c \
    kernel/trap/kernelvec.S \
    kernel/trap/trap.c \
    kernel/trap/timervec.S \
    kernel/trap/plic.c \
    kernel/proc/proc.c \
    kernel/proc/swtch.S \
    kernel/trap/trampoline.S \
    kernel/syscall/syscall.c \
    kernel/syscall/sysproc.c \
    kernel/proc/spinlock.c \
    kernel/trap/userret.S\
    kernel/fs/bio.c\
    kernel/fs/file.c\
    kernel/fs/fs.c\
    kernel/syscall/sysfile.c\
    kernel/driver/virtio_disk.c


KERNEL   = kernel.elf
LDSCRIPT = kernel.ld

# ============================================================
# 构建目标
# ============================================================
all: $(KERNEL)

# 【修复 1】：统一目标名和产出文件名
mkfs: mkfs.c
	gcc mkfs.c -o mkfs

# 生成镜像依赖 mkfs 工具
fs.img: mkfs
	@echo "正在创建并格式化磁盘镜像 fs.img..."
	./mkfs

initcode.out: initcode.S initcode.ld
	$(CC) $(CFLAGS) -nostdlib -nostartfiles -fno-pic -fno-pie -T initcode.ld -o initcode.out initcode.S

initcode.bin: initcode.out
	$(OBJCOPY) -S -O binary initcode.out initcode.bin

initcode.o: initcode.bin
	$(LD) -r -b binary -o initcode.o initcode.bin

# 【核心黑魔法 2】：内核编译依赖 initcode.o，并将其一起链接进去
$(KERNEL): $(SRCS) $(LDSCRIPT) initcode.o
	$(CC) $(CFLAGS) -T $(LDSCRIPT) $(SRCS) initcode.o -o $@
	@echo "======================================"
	@echo " 内核编译成功：$(KERNEL)"
	@echo " 现在运行 'make run' 启动 QEMU"
	@echo "======================================"

QEMU_DISK = -drive file=fs.img,if=none,format=raw,id=x0 \
            -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# 【修复 2】：让 run 依赖 fs.img，确保开机前硬盘一定存在
# 【修复 3】：删除了 $(QEMU_DISK) 前面多余的 "-"
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
	@echo ""
	@echo "QEMU 已暂停，等待 GDB 连接..."
	@echo "在新终端中运行："
	@echo "  gdb-multiarch $(KERNEL)"
	@echo "  (gdb) target remote :1234"
	@echo "  (gdb) break _entry"
	@echo "  (gdb) continue"
	@echo ""

# 【修复 4】：清理时顺手把 mkfs 和 fs.img 也删掉，保持干净
clean: 
	rm -f $(KERNEL) *.o *.d initcode_tmp.o initcode.out initcode.bin mkfs fs.img

.PHONY: all run debug clean