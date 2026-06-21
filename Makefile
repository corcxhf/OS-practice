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

BUILD_DIR = build
PORTS_DIR = ports
TINYCC_DIR = $(PORTS_DIR)/tinycc
NEATVI_DIR = $(PORTS_DIR)/neatvi
TESTS_DIR = tests
LIBC_CONTRACT_SOURCE = $(TESTS_DIR)/libc_contract.c
FS_CONTRACT_SOURCE = $(TESTS_DIR)/fs_contract.c
TTY_CONTRACT_SOURCE = $(TESTS_DIR)/tty_contract.c
TCC_USER_CC ?= riscv64-linux-gnu-gcc
TCC_USER_AR ?= riscv64-linux-gnu-ar
PORT_USER_CC ?= $(TCC_USER_CC)
TCC_RUNTIME_CC ?= $(CC)
TCC_RUNTIME_AR ?= $(CROSS)ar
TCC_USER = $(BUILD_DIR)/tcc
VI_USER = $(BUILD_DIR)/vi
TCC_PREDEFS = $(TINYCC_DIR)/tccdefs_.h
TCC_C2STR = $(BUILD_DIR)/c2str.exe
TCC_RUNTIME = $(BUILD_DIR)/crt1.o $(BUILD_DIR)/crti.o $(BUILD_DIR)/crtn.o $(BUILD_DIR)/libc.a
TCC_INCLUDE_DIR = $(TINYCC_DIR)/myos-include
TCC_HEADERS = $(wildcard $(TCC_INCLUDE_DIR)/*.h) $(wildcard $(TCC_INCLUDE_DIR)/sys/*.h)
UPROG_IMAGE_ARGS = $(foreach prog,$(UPROGS),$(prog)=/bin/$(notdir $(prog)))
TCC_IMAGE_ARGS = $(TCC_USER)=/bin/tcc $(foreach file,$(TCC_RUNTIME),$(file)=/lib/$(notdir $(file)))
TCC_HEADER_IMAGE_ARGS = $(foreach hdr,$(TCC_HEADERS),$(hdr)=/include/$(patsubst $(TCC_INCLUDE_DIR)/%,%,$(hdr)))
PORT_BINARY_IMAGE_ARGS = $(VI_USER)=/bin/vi
TEST_SOURCE_IMAGE_ARGS = $(LIBC_CONTRACT_SOURCE)=/src/tests/libc_ct.c $(FS_CONTRACT_SOURCE)=/src/tests/fs_ct.c $(TTY_CONTRACT_SOURCE)=/src/tests/tty_ct.c
TCC_RUNTIME_CFLAGS = -Os -nostdlib -fno-builtin -ffreestanding \
    -mno-relax -msmall-data-limit=0 -fno-pic -fno-pie \
    -mcmodel=medany -march=rv64gc -mabi=lp64d
TCC_USER_DEPS = \
    $(TCC_PREDEFS) \
    $(wildcard $(TINYCC_DIR)/*.c) \
    $(wildcard $(TINYCC_DIR)/*.h) \
    $(wildcard $(K)/include/*.h)
NEATVI_SRCS = $(filter-out $(NEATVI_DIR)/stag.c,$(wildcard $(NEATVI_DIR)/*.c))
NEATVI_DEPS = $(NEATVI_SRCS) $(wildcard $(NEATVI_DIR)/*.h)
TCC_USER_CFLAGS = -Os -static -nostdlib -fno-builtin \
    -mno-relax -msmall-data-limit=0 \
    -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-e,_start -Wl,-s \
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
    -I. -I$(abspath $(K)/include) \
    -DONE_SOURCE -DCONFIG_TCC_PREDEFS=1 -DTCC_TARGET_RISCV64
PORT_USER_CFLAGS = -Os -static -nostdlib -fno-builtin -ffreestanding \
    -mno-relax -msmall-data-limit=0 -fno-pic -fno-pie \
    -mcmodel=medany -march=rv64gc -mabi=lp64d \
    -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,-e,_start -Wl,-s \
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector \
    -I$(TCC_INCLUDE_DIR)

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
    $U/clear \
    $U/rm \
    $U/grep \
    $U/sbrktest 
#     $U/testpipe \

$U/%: $U/%.c
	$(CC) $(CFLAGS) $< -o $@

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

$(BUILD_DIR):
	mkdir -p $@

mkfs: mkfs.c
	gcc mkfs.c -o mkfs

$(TCC_C2STR): $(TINYCC_DIR)/conftest.c | $(BUILD_DIR)
	gcc -DC2STR $< -o $@

$(TCC_PREDEFS): $(TINYCC_DIR)/include/tccdefs.h $(TCC_C2STR)
	$(TCC_C2STR) $< $@

$(TCC_USER): $(TCC_USER_DEPS) | $(BUILD_DIR)
	cd $(TINYCC_DIR) && $(TCC_USER_CC) $(TCC_USER_CFLAGS) malloc.c glue.c tcc.c -o $(abspath $@) -lgcc

$(BUILD_DIR)/crt1.o: $(TINYCC_DIR)/myos-crt1.S | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/crti.o: $(TINYCC_DIR)/myos-crti.S | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/crtn.o: $(TINYCC_DIR)/myos-crtn.S | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/myos-libc.o: $(TINYCC_DIR)/myos-libc.c | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc.a: $(BUILD_DIR)/myos-libc.o | $(BUILD_DIR)
	$(TCC_RUNTIME_AR) rcs $@ $<

$(VI_USER): $(NEATVI_DEPS) $(TINYCC_DIR)/myos-crt1.S $(TINYCC_DIR)/myos-libc.c $(TCC_HEADERS) | $(BUILD_DIR)
	$(PORT_USER_CC) $(PORT_USER_CFLAGS) -I$(NEATVI_DIR) $(TINYCC_DIR)/myos-crt1.S $(NEATVI_SRCS) $(TINYCC_DIR)/myos-libc.c -o $@ -lgcc

fs.img: mkfs $(UPROGS) $(TCC_USER) $(TCC_RUNTIME) $(TCC_HEADERS) $(LIBC_CONTRACT_SOURCE) $(FS_CONTRACT_SOURCE) $(TTY_CONTRACT_SOURCE) $(VI_USER)
	@echo "正在创建并格式化磁盘镜像 fs.img..."
	./mkfs fs.img $(UPROG_IMAGE_ARGS) $(TCC_IMAGE_ARGS) $(TCC_HEADER_IMAGE_ARGS) $(PORT_BINARY_IMAGE_ARGS) $(TEST_SOURCE_IMAGE_ARGS)


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

test-qemu: $(KERNEL) fs.img
	python3 scripts/qemu_smoke.py --kernel $(KERNEL) --fs fs.img

clean: 
	rm -rf $(BUILD_DIR)
	rm -f $(KERNEL) tcc crt1.o crti.o crtn.o libc.a $(TCC_PREDEFS) *.o *.d $U/*.o $U/*.out $U/*.bin $U/*.d $(TINYCC_DIR)/myos-libc.o mkfs fs.img

.PHONY: all run debug test-qemu clean
