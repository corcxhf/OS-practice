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
ULIB = \
    $U/ulib.o \
    $U/usys.o

UPROGS = \
    $U/echo \
    $U/ls \
    $U/cat \
    $U/touch \
    $U/mkdir \
    $U/clear

$U/%.o: $U/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 2. 编译用户态 汇编 文件为 .o (针对 usys.S)
$U/%.o: $U/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# 3. 链接用户态程序：依赖对象的 .o，公共库 ULIB，以及用户态链接脚本 user.ld
$U/_%: $U/%.o $(ULIB) $U/user.ld
	$(LD) -T $U/user.ld -o $@ $< $(ULIB)
	$(OBJDUMP) -S $@ > $@.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $@.sym



all: $(KERNEL)

mkfs: mkfs.c
	gcc mkfs.c -o mkfs

fs.img: mkfs $(UPROGS)
	@echo "正在创建并格式化磁盘镜像 fs.img..."
	./mkfs fs.img $(UPROGS)


$U/initcode.out: $U/initcode.c $U/initcode.ld
	$(CC) $(CFLAGS) -mno-relax -nostdlib -nostartfiles -fno-pic -fno-pie -T $U/initcode.ld -o $@ $<

# 2. 剥离 ELF 头部，提取纯二进制指令和数据
$U/initcode.bin: $U/initcode.out
	$(OBJCOPY) -S -O binary $< $@

# 3. 重新包装为含有 _binary_... 符号的内核依赖对象
$U/initcode.o: $U/initcode.bin
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