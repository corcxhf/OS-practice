

qemu:
	riscv64-unknown-elf-gcc -nostdlib -fno-builtin -mcmodel=medany -T kernel.ld kernel/boot/entry.S kernel/driver/uart.c kernel/boot/main.c -o kernel.elf
	qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf -nographic