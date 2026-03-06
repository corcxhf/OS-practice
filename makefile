CC = riscv64-unknown-elf-gcc
CFLAGS = -g -O0 -nostdlib -fno-builtin -mcmodel=medany

SRCS = kernel/boot/entry.S kernel/driver/uart.c kernel/boot/main.c
QEMU = qemu-system-riscv64
QEMU_OPTS = -machine virt -bios none -kernel kernel.elf

all: kernel.elf

kernel.elf: $(SRCS) kernel.ld
	$(CC) $(CFLAGS) -T kernel.ld $(SRCS) -o $@

qemu: kernel.elf
	$(QEMU) $(QEMU_OPTS) -nographic

debug: kernel.elf
	@$(QEMU) $(QEMU_OPTS) -display none -serial file:qemu.log -S -s & \
	PID=$$!; \
	gdb-multiarch -q -iex "set auto-load safe-path /" kernel.elf; \
	kill $$PID 2>/dev/null || true

clean:
	rm -f kernel.elf qemu.log