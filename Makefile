# Makefile — 构建系统
# ============================================================
# 工具链配置
# ============================================================
CROSS   = riscv64-unknown-elf-
CC      = $(CROSS)gcc
CXX     = $(CROSS)g++
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
MYOS_PORT_DIR = $(PORTS_DIR)/myos
TESTS_DIR = tests
USERLAND_DIR = userland
LIBC_CONTRACT_SOURCE = $(TESTS_DIR)/libc_contract.c
FS_CONTRACT_SOURCE = $(TESTS_DIR)/fs_contract.c
TTY_CONTRACT_SOURCE = $(TESTS_DIR)/tty_contract.c
GCC_STATIC_MAIN_SOURCE = $(TESTS_DIR)/gcc_static_main.c
GCC_STATIC_LIB_SOURCE = $(TESTS_DIR)/gcc_static_lib.c
GCC_STATIC_HEADER_SOURCE = $(TESTS_DIR)/gcc_static_lib.h
CXX_CONTRACT_SOURCE = $(TESTS_DIR)/cxx_contract.cc
CXX_STD_CONTRACT_SOURCE = $(TESTS_DIR)/cxx_std_contract.cc
CXX_DYN_CONTRACT_SOURCE = $(TESTS_DIR)/cxx_dyn_contract.cc
GXX_CONTRACT_SOURCE = $(TESTS_DIR)/gxx_contract.cc
USERLAND_SOURCES = $(wildcard $(USERLAND_DIR)/*.c)
BUILD_DESCRIPTION = Buildfile
TCC_USER_CC ?= riscv64-linux-gnu-gcc
TCC_USER_AR ?= riscv64-linux-gnu-ar
PORT_USER_CC ?= $(TCC_USER_CC)
TCC_RUNTIME_CC ?= $(CC)
TCC_RUNTIME_AR ?= $(CROSS)ar
TCC_USER = $(BUILD_DIR)/tcc
VI_USER = $(BUILD_DIR)/vi
CXX_CONTRACT_USER = $(BUILD_DIR)/cxxtest
CXX_STD_CONTRACT_USER = $(BUILD_DIR)/cxxstdtest
CXX_DYN_CONTRACT_USER = $(BUILD_DIR)/cxxdyntest
GXX_CONTRACT_USER = $(BUILD_DIR)/gxxtest
GCC_CONTRACT_USER = $(BUILD_DIR)/gcctest
GCC_STATIC_MAIN_OBJ = $(BUILD_DIR)/gcc_static_main.o
GCC_STATIC_LIB_OBJ = $(BUILD_DIR)/gcc_static_lib.o
GCC_STATIC_CONTRACT_USER = $(BUILD_DIR)/gccstatictest
GCC_USERLAND_USERS = \
    $(BUILD_DIR)/gcc-hello \
    $(BUILD_DIR)/gcc-lines \
    $(BUILD_DIR)/gcc-cat \
    $(BUILD_DIR)/gcc-wc \
    $(BUILD_DIR)/gcc-grep \
    $(BUILD_DIR)/gcc-diff
TCC_PREDEFS = $(TINYCC_DIR)/tccdefs_.h
TCC_C2STR = $(BUILD_DIR)/c2str.exe
TCC_RUNTIME = $(BUILD_DIR)/crt1.o $(BUILD_DIR)/crti.o $(BUILD_DIR)/crtn.o $(BUILD_DIR)/libc.a
TCC_INCLUDE_DIR = $(MYOS_PORT_DIR)/include
TCC_HEADERS = $(wildcard $(TCC_INCLUDE_DIR)/*.h) $(wildcard $(TCC_INCLUDE_DIR)/sys/*.h)
MYOS_CRT1 = $(MYOS_PORT_DIR)/crt1.S
MYOS_CRTI = $(MYOS_PORT_DIR)/crti.S
MYOS_CRTN = $(MYOS_PORT_DIR)/crtn.S
MYOS_LIBC = $(MYOS_PORT_DIR)/libc.c
CXX_INCLUDE_DIR = $(PORTS_DIR)/cxx/include
CXX_HEADERS = $(wildcard $(CXX_INCLUDE_DIR)/*)
CXX_RUNTIME_HEADERS = $(PORTS_DIR)/cxx/myos-cxx-sys.h
CXX_CRT0 = $(BUILD_DIR)/cxx-crt0.o
CXX_RUNTIME = $(BUILD_DIR)/libcxxrt.a
MYOS_SYSROOT = $(BUILD_DIR)/sysroot
MYOS_GCC = $(BUILD_DIR)/riscv64-myos-gcc
CXX_SYSROOT = $(BUILD_DIR)/cxx-sysroot
CXX_MYOS_GXX = $(BUILD_DIR)/riscv64-myos-g++
UPROG_IMAGE_ARGS = $(foreach prog,$(UPROGS),$(prog)=/bin/$(notdir $(prog)))
TCC_IMAGE_ARGS = $(TCC_USER)=/bin/tcc $(foreach file,$(TCC_RUNTIME),$(file)=/lib/$(notdir $(file)))
TCC_HEADER_IMAGE_ARGS = $(foreach hdr,$(TCC_HEADERS),$(hdr)=/include/$(patsubst $(TCC_INCLUDE_DIR)/%,%,$(hdr)))
CXX_HEADER_IMAGE_ARGS = $(foreach hdr,$(CXX_HEADERS),$(hdr)=/include/c++/$(patsubst $(CXX_INCLUDE_DIR)/%,%,$(hdr)))
PORT_BINARY_IMAGE_ARGS = $(VI_USER)=/bin/vi
TEST_SOURCE_IMAGE_ARGS = $(LIBC_CONTRACT_SOURCE)=/src/tests/libc_ct.c $(FS_CONTRACT_SOURCE)=/src/tests/fs_ct.c $(TTY_CONTRACT_SOURCE)=/src/tests/tty_ct.c
GCC_CONTRACT_IMAGE_ARGS = $(GCC_CONTRACT_USER)=/bin/gcctest
GCC_STATIC_CONTRACT_IMAGE_ARGS = $(GCC_STATIC_CONTRACT_USER)=/bin/gccstatictest $(GCC_STATIC_MAIN_SOURCE)=/src/tests/gccst_main.c $(GCC_STATIC_LIB_SOURCE)=/src/tests/gccst_lib.c $(GCC_STATIC_HEADER_SOURCE)=/src/tests/gccst_lib.h
GCC_USERLAND_IMAGE_ARGS = $(foreach prog,$(GCC_USERLAND_USERS),$(prog)=/bin/$(notdir $(prog)))
CXX_CONTRACT_IMAGE_ARGS = $(CXX_CONTRACT_USER)=/bin/cxxtest $(CXX_CONTRACT_SOURCE)=/src/tests/cxx_ct.cc
CXX_STD_CONTRACT_IMAGE_ARGS = $(CXX_STD_CONTRACT_USER)=/bin/cxxstdtest $(CXX_STD_CONTRACT_SOURCE)=/src/tests/cxxstd_ct.cc
CXX_DYN_CONTRACT_IMAGE_ARGS = $(CXX_DYN_CONTRACT_USER)=/bin/cxxdyntest $(CXX_DYN_CONTRACT_SOURCE)=/src/tests/cxxdyn_ct.cc
GXX_CONTRACT_IMAGE_ARGS = $(GXX_CONTRACT_USER)=/bin/gxxtest $(GXX_CONTRACT_SOURCE)=/src/tests/gxx_ct.cc
CXX_RUNTIME_IMAGE_ARGS = $(CXX_RUNTIME)=/lib/libcxxrt.a
USERLAND_SOURCE_IMAGE_ARGS = $(foreach src,$(USERLAND_SOURCES),$(src)=/src/userland/$(notdir $(src)))
BUILD_DESCRIPTION_IMAGE_ARGS = $(BUILD_DESCRIPTION)=/src/Buildfile
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
CXX_CONTRACT_CXXFLAGS = -Os -nostdlib -nostartfiles -ffreestanding \
    -std=c++20 \
    -fno-exceptions -fno-rtti -fno-use-cxa-atexit \
    -fno-stack-protector -fno-threadsafe-statics \
    -mno-relax -msmall-data-limit=0 -fno-pic -fno-pie \
    -mcmodel=medany -march=rv64gc -mabi=lp64d \
    -I$(PORTS_DIR)/cxx \
    -Wl,-e,_start -Wl,--gc-sections -Wl,-s
CXX_STD_CONTRACT_CXXFLAGS = $(CXX_CONTRACT_CXXFLAGS) -nostdinc++ -I$(CXX_INCLUDE_DIR)
CXX_RUNTIME_CXXFLAGS = $(CXX_STD_CONTRACT_CXXFLAGS) -c

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
    $U/wc \
    $U/cp \
    $U/install \
    $U/mv \
    $U/cmp \
    $U/diff \
    $U/head \
    $U/tail \
    $U/hexdump \
    $U/runtests \
    $U/build \
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

$(BUILD_DIR)/crt1.o: $(MYOS_CRT1) | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/crti.o: $(MYOS_CRTI) | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/crtn.o: $(MYOS_CRTN) | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/myos-libc.o: $(MYOS_LIBC) $(TCC_HEADERS) | $(BUILD_DIR)
	$(TCC_RUNTIME_CC) $(TCC_RUNTIME_CFLAGS) -c $< -o $@

$(BUILD_DIR)/libc.a: $(BUILD_DIR)/myos-libc.o | $(BUILD_DIR)
	$(TCC_RUNTIME_AR) rcs $@ $<

$(VI_USER): $(NEATVI_DEPS) $(MYOS_CRT1) $(MYOS_LIBC) $(TCC_HEADERS) | $(BUILD_DIR)
	$(PORT_USER_CC) $(PORT_USER_CFLAGS) -I$(NEATVI_DIR) $(MYOS_CRT1) $(NEATVI_SRCS) $(MYOS_LIBC) -o $@ -lgcc

$(CXX_CRT0): $(PORTS_DIR)/cxx/myos-cxx-crt0.cc $(CXX_RUNTIME_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXX_RUNTIME_CXXFLAGS) $< -o $@

$(BUILD_DIR)/myos-cxx-runtime.o: $(PORTS_DIR)/cxx/myos-cxx-runtime.cc $(CXX_RUNTIME_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXX_RUNTIME_CXXFLAGS) $< -o $@

$(CXX_RUNTIME): $(BUILD_DIR)/myos-cxx-runtime.o | $(BUILD_DIR)
	$(TCC_RUNTIME_AR) rcs $@ $<

$(CXX_CONTRACT_USER): $(CXX_CONTRACT_SOURCE) $(CXX_CRT0) $(CXX_RUNTIME) $(TCC_RUNTIME) | $(BUILD_DIR)
	$(CXX) $(CXX_CONTRACT_CXXFLAGS) $(CXX_CRT0) $< $(CXX_RUNTIME) $(BUILD_DIR)/libc.a -o $@

$(CXX_STD_CONTRACT_USER): $(CXX_STD_CONTRACT_SOURCE) $(CXX_HEADERS) $(CXX_CRT0) $(CXX_RUNTIME) $(TCC_RUNTIME) | $(BUILD_DIR)
	$(CXX) $(CXX_STD_CONTRACT_CXXFLAGS) $(CXX_CRT0) $< $(CXX_RUNTIME) $(BUILD_DIR)/libc.a -o $@

$(CXX_DYN_CONTRACT_USER): $(CXX_DYN_CONTRACT_SOURCE) $(CXX_HEADERS) $(CXX_CRT0) $(CXX_RUNTIME) $(TCC_RUNTIME) | $(BUILD_DIR)
	$(CXX) $(CXX_STD_CONTRACT_CXXFLAGS) $(CXX_CRT0) $< $(CXX_RUNTIME) $(BUILD_DIR)/libc.a -o $@

gcc-sysroot: $(MYOS_SYSROOT)/.stamp $(MYOS_GCC)

$(MYOS_SYSROOT)/.stamp: $(TCC_RUNTIME) $(TCC_HEADERS) | $(BUILD_DIR)
	rm -rf $(MYOS_SYSROOT)
	mkdir -p $(MYOS_SYSROOT)/include $(MYOS_SYSROOT)/lib
	cp -R $(TCC_INCLUDE_DIR)/* $(MYOS_SYSROOT)/include/
	cp $(TCC_RUNTIME) $(MYOS_SYSROOT)/lib/
	touch $@

$(MYOS_GCC): scripts/riscv64-myos-gcc.sh $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	cp $< $@
	chmod +x $@

$(GCC_CONTRACT_USER): $(LIBC_CONTRACT_SOURCE) $(MYOS_GCC) $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	$(MYOS_GCC) $< -o $@

$(GCC_STATIC_MAIN_OBJ): $(GCC_STATIC_MAIN_SOURCE) $(GCC_STATIC_HEADER_SOURCE) $(MYOS_GCC) $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	$(MYOS_GCC) -c $< -o $@

$(GCC_STATIC_LIB_OBJ): $(GCC_STATIC_LIB_SOURCE) $(GCC_STATIC_HEADER_SOURCE) $(MYOS_GCC) $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	$(MYOS_GCC) -c $< -o $@

$(GCC_STATIC_CONTRACT_USER): $(GCC_STATIC_MAIN_OBJ) $(GCC_STATIC_LIB_OBJ) $(MYOS_GCC) $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	$(MYOS_GCC) $(GCC_STATIC_MAIN_OBJ) $(GCC_STATIC_LIB_OBJ) -o $@

$(BUILD_DIR)/gcc-%: $(USERLAND_DIR)/%.c $(MYOS_GCC) $(MYOS_SYSROOT)/.stamp | $(BUILD_DIR)
	$(MYOS_GCC) $< -o $@

cxx-sysroot: $(CXX_SYSROOT)/.stamp $(CXX_MYOS_GXX)

$(CXX_SYSROOT)/.stamp: $(CXX_CRT0) $(CXX_RUNTIME) $(CXX_RUNTIME_HEADERS) $(CXX_HEADERS) $(TCC_HEADERS) | $(BUILD_DIR)
	rm -rf $(CXX_SYSROOT)
	mkdir -p $(CXX_SYSROOT)/include $(CXX_SYSROOT)/include/c++ $(CXX_SYSROOT)/lib
	cp -R $(TCC_INCLUDE_DIR)/* $(CXX_SYSROOT)/include/
	cp $(CXX_HEADERS) $(CXX_SYSROOT)/include/c++/
	cp $(CXX_RUNTIME_HEADERS) $(CXX_SYSROOT)/include/myos-cxx-sys.h
	cp $(CXX_CRT0) $(CXX_SYSROOT)/lib/cxx-crt0.o
	cp $(CXX_RUNTIME) $(CXX_SYSROOT)/lib/libcxxrt.a
	cp $(TCC_RUNTIME) $(CXX_SYSROOT)/lib/
	touch $@

$(CXX_MYOS_GXX): scripts/riscv64-myos-g++.sh $(CXX_SYSROOT)/.stamp | $(BUILD_DIR)
	cp $< $@
	chmod +x $@

$(GXX_CONTRACT_USER): $(GXX_CONTRACT_SOURCE) $(CXX_MYOS_GXX) $(CXX_SYSROOT)/.stamp | $(BUILD_DIR)
	$(CXX_MYOS_GXX) $< -o $@

fs.img: mkfs $(UPROGS) $(TCC_USER) $(TCC_RUNTIME) $(TCC_HEADERS) $(GCC_CONTRACT_USER) $(GCC_STATIC_CONTRACT_USER) $(GCC_USERLAND_USERS) $(CXX_HEADERS) $(CXX_RUNTIME) $(LIBC_CONTRACT_SOURCE) $(FS_CONTRACT_SOURCE) $(TTY_CONTRACT_SOURCE) $(GCC_STATIC_MAIN_SOURCE) $(GCC_STATIC_LIB_SOURCE) $(GCC_STATIC_HEADER_SOURCE) $(CXX_CONTRACT_SOURCE) $(CXX_CONTRACT_USER) $(CXX_STD_CONTRACT_SOURCE) $(CXX_STD_CONTRACT_USER) $(CXX_DYN_CONTRACT_SOURCE) $(CXX_DYN_CONTRACT_USER) $(GXX_CONTRACT_SOURCE) $(GXX_CONTRACT_USER) $(USERLAND_SOURCES) $(BUILD_DESCRIPTION) $(VI_USER)
	@echo "正在创建并格式化磁盘镜像 fs.img..."
	./mkfs fs.img $(UPROG_IMAGE_ARGS) $(TCC_IMAGE_ARGS) $(TCC_HEADER_IMAGE_ARGS) $(GCC_CONTRACT_IMAGE_ARGS) $(GCC_STATIC_CONTRACT_IMAGE_ARGS) $(GCC_USERLAND_IMAGE_ARGS) $(CXX_HEADER_IMAGE_ARGS) $(CXX_RUNTIME_IMAGE_ARGS) $(PORT_BINARY_IMAGE_ARGS) $(TEST_SOURCE_IMAGE_ARGS) $(CXX_CONTRACT_IMAGE_ARGS) $(CXX_STD_CONTRACT_IMAGE_ARGS) $(CXX_DYN_CONTRACT_IMAGE_ARGS) $(GXX_CONTRACT_IMAGE_ARGS) $(USERLAND_SOURCE_IMAGE_ARGS) $(BUILD_DESCRIPTION_IMAGE_ARGS)


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
	rm -f $(KERNEL) tcc crt1.o crti.o crtn.o libc.a $(TCC_PREDEFS) *.o *.d $U/*.o $U/*.out $U/*.bin $U/*.d mkfs fs.img

.PHONY: all run debug test-qemu clean gcc-sysroot cxx-sysroot
