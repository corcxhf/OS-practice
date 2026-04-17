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
    kernel/trap/userret.S

KERNEL   = kernel.elf
LDSCRIPT = kernel.ld

# ============================================================
# 构建目标
# ============================================================
all: $(KERNEL)

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

# 在 QEMU 中运行内核
run: $(KERNEL)
	qemu-system-riscv64 \
        -machine virt \
        -bios none \
        -kernel $(KERNEL) \
        -nographic

# 启动 QEMU 并暂停，等待 GDB 连接（调试模式）
debug: $(KERNEL)
	-pkill -f qemu-system-riscv64
	qemu-system-riscv64 \
        -machine virt \
        -bios none \
        -kernel $(KERNEL) \
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

# 清除编译产物（加入了提取二进制时产生的临时文件）
clean:
	rm -f $(KERNEL) *.o *.d initcode_tmp.o initcode.out initcode.bin

.PHONY: all run debug clean