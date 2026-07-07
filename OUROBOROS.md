# Project Ouroboros: A Self-Forging Unix

## Vision

Project Ouroboros turns MyOS from a teaching kernel into a self-hosting,
self-testing, self-upgrading Unix-like system.

The end state is not merely "MyOS can run programs." The end state is:

```text
MyOS can edit, compile, test, package, and boot the next MyOS from inside MyOS.
```

The final target interaction should feel like this:

```sh
cd /src/myos
vi kernel/syscall/sysfile.c
make test
make world
reboot --next /boot/myos-next
```

## North Star

MyOS should become the workshop that forges its own successor.

That means the system eventually needs:

- A reliable shell and process model.
- A usable editor.
- A C compiler, assembler/linker path, runtime objects, and libc.
- A persistent filesystem large enough for source trees and build products.
- A build tool.
- A regression test harness.
- A way to install or boot a newly built system image.
- A recovery path when the new system fails.

## Current Position

Current stage:

```text
Spark -> Workbench
```

Already present:

- Basic kernel boot.
- Shell with `fork`, `exec`, `wait`, PATH lookup, pipes, and redirection.
- File operations such as `cat`, `ls`, `touch`, `mkdir`, `rm`.
- `vi`/`edit` based on neatvi.
- `tcc` and minimal C runtime objects.
- A growing libc.
- QEMU smoke testing through `make test-qemu`.

Still fragile:

- syscall/libc contracts are incomplete.
- TTY behavior is partly patched rather than fully modeled.
- Filesystem truncation/unlink/block reuse semantics need hardening.
- Shell grammar is still small.
- Build tooling inside MyOS does not exist yet.
- MyOS cannot yet build its own userland or kernel from inside itself.

## Stage 0: Contract Bedrock

Goal: every important kernel/userspace contract has an automated QEMU test.

Why this comes first:

If MyOS is going to evolve into a self-forging system, each feature must become
a contract before the next layer depends on it.

Deliverables:

- Keep `make test-qemu` fast and reliable.
- Add contract tests for:
  - shell behavior
  - filesystem behavior
  - syscall behavior
  - libc behavior
  - TTY behavior
  - compiler behavior
- Make every bug fix add or update a regression test.

Acceptance:

```sh
make test-qemu
```

passes from a clean build and catches regressions in `vi`, `tcc`, shell
redirection, file truncation, and interactive input.

Immediate checklist:

- [x] Add QEMU smoke test runner.
- [x] Cover basic shell, `vi`, and `tcc + scanf`.
- [x] Add a libc contract program compiled inside MyOS.
- [x] Add filesystem truncate/unlink/recreate stress tests.
- [x] Add shell pipeline and redirection edge cases.
- [x] Add TTY mode-switch and control-key tests.

## Stage 1: Workbench

Goal: MyOS is a comfortable place to write, compile, and run normal C programs.

Core user story:

```sh
vi hello.c
tcc hello.c -o hello
./hello
```

Work items:

- Stabilize `vi`:
  - normal/insert mode transitions
  - arrow keys
  - Backspace/Delete
  - save/quit behavior
  - large-ish files
- Stabilize `tcc`:
  - compile single C files
  - link with `/lib/crt*.o` and `/lib/libc.a`
  - emit runnable binaries
- Extend libc:
  - `printf`, `scanf`, `sscanf`
  - `fopen`, `fread`, `fwrite`, `fclose`
  - `lseek`, `stat`, `fstat`
  - `errno`
  - `malloc`, `free`, `realloc`
- Stabilize shell basics:
  - command execution
  - PATH
  - `>`
  - `|`
  - exit status reporting

Acceptance:

Inside MyOS:

```sh
vi calc.c
tcc calc.c -o calc
./calc
```

works repeatedly across rebooted QEMU sessions.

## Stage 2: Toolsmith

Goal: MyOS can build useful Unix-style tools inside itself.

Target tools:

- `grep`
- `wc`
- `cp`
- `mv`
- `head`
- `tail`
- `cmp`
- `diff` minimal version (present as `/bin/diff`)
- `date` or simple clock display if time exists
- `hexdump`

Work items:

- Fill libc gaps that real tools need.
- Improve shell argument handling.
- Add input redirection `<`.
- Add append redirection `>>`.
- Add multi-stage pipelines.
- Add a basic test runner inside MyOS.

Acceptance:

Inside MyOS:

```sh
vi grep.c
tcc grep.c -o grep
./grep main /src/*.c
```

or an equivalent source-search workflow works.

## Stage 3: Forge

Goal: MyOS can build and package its own userland.

This means the system can compile a suite of user programs from source and place
them into a runnable filesystem image or install tree.

Work items:

- Create an in-MyOS build description format.
  - first version: `/src/Buildfile` with `TARGET RECIPE SOURCE OUTPUT GROUP DEPS`
- Implement a small `make`-like tool:
  - targets
  - dependencies (`@target` dependencies are built before the current target)
  - recipes (`cc` and `copy`)
  - aggregate targets (`phony`)
  - first build/install loop: `build userland && build install` installs `/bin/hello`, `/bin/lines`, and self-built `/bin/cat2`, `/bin/diff2`, `/bin/wc2`, `/bin/grep2`
  - first world entry point: `build world`
  - first self-check entry point: `runtests world`
  - commands
  - timestamp or content checks
- Teach the filesystem to handle more source files and build artifacts.
  - first cut: mkfs now provisions a larger inode table for source/build/install churn.
- Add install paths:
  - `/bin`
  - `/lib`
  - `/include`
  - `/src`
  - `/tmp`
  - first install tool: `/bin/install SRC DST`
- Add a package or archive format.

Acceptance:

Inside MyOS:

```sh
cd /src/userland
make
make install
```

rebuilds and installs a meaningful set of `/bin` programs.

## Stage 4: Mirror

Goal: MyOS contains its own source tree and can inspect, edit, test, and rebuild
large parts of itself.

Work items:

- Expand filesystem capacity.
- Make directories and path traversal robust.
- Add recursive copy/import tools.
- Add archive extraction.
- Add source tree layout under `/src/myos`.
- Add enough editor and shell ergonomics to work in that tree.
- Add cross-stage tests that compare host-built and MyOS-built userland tools.

Acceptance:

Inside MyOS:

```sh
cd /src/myos
grep sys_open kernel/syscall/*.c
vi kernel/syscall/sysfile.c
make test
```

is a realistic workflow, not a demo script.

## Stage 5: Kernel Forge

Goal: MyOS can build a new kernel image from inside itself.

This is the major technical jump.

Likely requirements:

- Compiler support for the kernel's C subset.
- Assembler support or a strategy for prebuilt assembly objects.
- Linker script support.
- ELF generation and inspection.
- Enough memory for compiling and linking.
- Enough filesystem capacity for object files and kernel images.
- A stable runtime for long-running build jobs.

Possible stepping stones:

1. Build small user programs inside MyOS.
2. Build all user programs inside MyOS.
3. Build selected kernel C objects inside MyOS.
4. Link a toy kernel inside MyOS.
5. Link the real MyOS kernel inside MyOS.

Acceptance:

Inside MyOS:

```sh
cd /src/myos
make kernel
ls /boot/myos-next
```

produces a bootable kernel candidate.

## Stage 6: Phoenix

Goal: MyOS can safely boot a system image that it produced itself.

Work items:

- Define `/boot` layout.
- Add a boot candidate mechanism:
  - current kernel
  - next kernel
  - previous known-good kernel
- Add a reboot command or bootloader handoff.
- Add automatic post-boot health checks.
- Add rollback if the new image fails.

Acceptance:

Inside MyOS:

```sh
make world
make install-next
reboot --next
```

boots into the newly built system, runs health checks, and can roll back.

## Stage 7: Ouroboros

Goal: MyOS completes the self-hosting loop.

The full loop:

```text
edit MyOS in MyOS
compile MyOS in MyOS
test MyOS in MyOS
package MyOS in MyOS
boot the new MyOS
repeat
```

Acceptance:

The project can demonstrate two generations:

```text
host-built MyOS -> MyOS-built MyOS -> MyOS-built successor
```

The second generation must be produced by tools running inside the first
generation.

## Engineering Rules

Every stage follows these rules:

- A feature is not done until it has a QEMU regression test.
- syscall semantics must be documented by tests, not memory.
- shell/editor/compiler workflows are treated as first-class product flows.
- Prefer small Unix-like tools over large monolithic features.
- Keep a working image available before risky filesystem or boot changes.
- When a bug appears, first make it reproducible, then fix it, then preserve it
  as a regression test.

## Near-Term Roadmap

Next ten concrete steps:

1. Add `tests/libc_contract.c` and compile/run it inside QEMU.
2. Test `open`, `O_TRUNC`, `lseek`, `fread`, `fwrite`, `scanf`, `printf`.
3. Add filesystem tests for unlink/recreate/truncate.
4. Add `stat` and `fstat` with real file size and type.
5. Add `O_APPEND` and test `>>`.
6. Add shell input redirection `<`.
7. Add `cp` and `wc`.
8. Add multi-stage pipelines.
9. Add a simple in-MyOS test runner.
10. Add a tiny `make` prototype for user programs.

## Motto

```text
Not just an OS that runs programs.
An OS that learns to make itself.
```
