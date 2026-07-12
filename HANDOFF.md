# Handoff: MyOS GCC Driver / Binutils Port

Updated: 2026-07-08

## Current Goal

We are not implementing GCC from scratch. The goal is to make MyOS host a real GCC-style toolchain by porting the surrounding pieces and exposing the contracts that GCC expects:

- GNU binutils inside MyOS: assembler, linker, archives, symbol/inspect tools
- A small POSIX/libc/kernel surface that lets those programs run
- A GCC-like driver path that eventually invokes real GCC `cc1`, native `as`, and native `ld`

The default `fs.img` still keeps stable wrapper slots. The larger experimental image is `fs-binutils.img`, where `/bin/myos-*` contains the real GNU binutils ports and `/bin/myos-gcc` is the experimental driver.

## What Works Now

- `make binutils-as` builds GNU gas as `build/myos-as`.
- `make binutils-ld` builds GNU ld as `build/myos-ld`.
- `make binutils-ar`, `binutils-ranlib`, `binutils-nm`, `binutils-strip`, `binutils-objcopy`, `binutils-objdump`, and `binutils-readelf` build native GNU tools.
- `make fs-binutils.img` packages:
  - `/bin/myos-as`, `/bin/myos-ld`
  - `/bin/myos-ar`, `/bin/myos-ranlib`, `/bin/myos-nm`, `/bin/myos-strip`, `/bin/myos-objcopy`, `/bin/myos-objdump`, `/bin/myos-readelf`
  - aliases `/bin/ar`, `/bin/ranlib`, `/bin/nm`, `/bin/strip`, `/bin/objcopy`, `/bin/objdump`, `/bin/readelf`
  - `/bin/myos-gcc`, `/bin/cpp`, `/lib/libgcc.a`, `/obj/libgcc_need.o`
- `/bin/gcc`, `/bin/as`, and `/bin/ld` first try the native experimental tool when present, then fall back to the stable tcc-backed slots.
- `/bin/myos-gcc` can compile C through `/bin/tcc`, assemble through `/bin/myos-as`, and link through `/bin/myos-ld`.
- `/bin/myos-gcc -E` and `/bin/cpp` provide a preprocessor slot backed by `/bin/tcc -E`.
- `/bin/myos-gcc` supports `.s` and preprocessed `.S` assembly inputs, including `-D/-I/-include/-isystem/-nostdinc` forwarding for the preprocess step.
- `/bin/myos-gcc` reports the expected tool/file paths for `-print-prog-name=as`, `ld`, `cpp`, `ar`, `ranlib`, plus `-print-libgcc-file-name` and basic `-print-file-name=*` runtime objects.

## Verified Commands

These pass as of this handoff:

```sh
make test-qemu
make test-binutils-as test-binutils-ld test-binutils-driver \
  test-binutils-archive test-binutils-libgcc test-binutils-search \
  test-binutils-inspect
```

The latest full binutils smoke run covered gas, ld, driver, archive tools, libgcc linking, library search, and inspect tools.

## Key Implementation Notes

1. Native binutils are real GNU binutils 2.42 cross-built for MyOS.
   - BFD TLS is disabled through `build/binutils-myos/bfd/config.cache` using empty `ac_cv_tls`.
   - `fs-binutils.img` is intentionally larger than `fs.img`.

2. The experimental driver is not real GCC yet.
   - `user/myos-gcc.c` is a small driver.
   - C compilation still uses `/bin/tcc`.
   - Assembly and linking use `/bin/myos-as` and `/bin/myos-ld`.
   - Preprocessing uses `/bin/tcc -E`.

3. POSIX/libc surface already added for binutils/GCC-adjacent work includes:
   - `fork`, `wait`, `exec`, `pipe`, `dup`, `dup2`, `fcntl`
   - `open`, `read`, `write`, `close`, `lseek`, `ftruncate`, `rename`, `getcwd`
   - `stat/fstat` with stable time fields, `access`, `mkstemp`, `mkdtemp`, `tmpfile`
   - `gettimeofday`, `getrusage`, `getrlimit`, `setrlimit`, `sysconf`, `getpagesize`
   - minimal `mmap/munmap` backed by malloc/free
   - `execvp()` PATH search

4. Kernel/runtime changes already matter for this work.
   - User-mode RISC-V floating point is enabled.
   - Trapframe saves/restores `f0`-`f31` and `fcsr`.
   - `mkfs` supports larger images and duplicate host-source hardlinks.

5. TCC is vendored in the main repository.
   - Its pinned upstream revision is documented in `ports/tinycc/UPSTREAM.md`.
   - The MyOS tcc shim has a required `glue.c` fix for tiny `vsnprintf()` so `tcc -E -DVALUE=... -o file` does not trap.
   - Update the vendored tree deliberately and preserve the MyOS-specific runtime and glue changes.

## Manual Commands Inside MyOS

Use the binutils image:

```sh
make fs-binutils.img
qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf \
  -drive file=fs-binutils.img,if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 -nographic
```

Then at `MyOS:/>`:

```sh
myos-as --version
myos-ld --version
myos-gcc --version
gcc --version
myos-gcc -print-prog-name=as
myos-gcc -print-prog-name=ld
myos-gcc -print-prog-name=cpp
myos-gcc -print-libgcc-file-name
```

Preprocessor slot:

```sh
vi pp.c
```

File contents:

```c
#ifndef VALUE
#define VALUE 1
#endif
int pp_value = VALUE;
```

Then:

```sh
myos-gcc -E -DVALUE=123 pp.c -o pp.i
cat pp.i
cpp -DVALUE=456 pp.c -o pp2.i
cat pp2.i
```

Native driver compile/link:

```sh
vi drv.c
```

File contents:

```c
#include <stdio.h>
int main(){ printf("DRIVER:%d\n", 321); return 0; }
```

Then:

```sh
myos-gcc drv.c -o drvbin
./drvbin
gcc drv.c -o gccbin
./gccbin
```

Preprocessed assembly:

```sh
vi x.S
```

File contents:

```asm
#define STATUS_CODE VALUE
.global _start
.text
_start:
 li a7,2
 li a0,STATUS_CODE
 ecall
```

Then:

```sh
myos-gcc -DVALUE=45 -c x.S -o x.o
myos-gcc -nostdlib x.o -o x
./x
echo $?
```

Expected status is `45`.

Libgcc search/link:

```sh
myos-gcc /obj/libgcc_need.o -o libgccrun
./libgccrun ok
echo $?
```

Expected output starts with `LIBGCC:` and status is `0`.

Inspect tools:

```sh
myos-nm drvbin
readelf -h drvbin
objdump -t drvbin
strip -o drvstrip drvbin
./drvstrip
```

## Next Recommended Steps

1. Start the real GCC port audit.
   - Add a build target for a minimal GCC source tree or GCC driver/`cc1` subset against the MyOS sysroot.
   - Let configure/build/runtime failures tell us the next missing libc/POSIX pieces.

2. Improve the subprocess contract for GCC/libiberty.
   - GCC driver code usually leans on libiberty/pex-like process handling.
   - Pipes, stderr/stdout redirection, temp files, wait status encoding, signals, and `errno` quality will matter.

3. Keep replacing tcc-backed pieces.
   - Today: `myos-gcc` uses tcc for C and preprocessing.
   - Target: real GCC `cc1` for C, then later `cc1plus` for C++.

## Repo State Notes

- `build/` is ignored and can be rebuilt.
- `fs.img`, `fs-binutils.img`, `kernel.elf`, `mkfs`, and user binaries are generated artifacts and are ignored by the repository.
- `scripts/__pycache__/` is untracked and can be removed before commit.
- The worktree contains older user/session changes unrelated to this handoff; do not blindly revert them.
