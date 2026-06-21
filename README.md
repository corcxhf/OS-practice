# MyOS

MyOS 是一个小型 RISC-V 类 Unix 操作系统。它起点是教学内核，但现在的目标已经升级为一个可以逐步自举的开发环境。

目标不是：

```text
MyOS 能运行程序。
```

而是：

```text
MyOS 能锻造下一代 MyOS。
```

完整长期路线见 [OUROBOROS.md](OUROBOROS.md)。

## 当前状态

MyOS 目前处在 **Spark -> Workbench** 之间：

- 内核可以在 QEMU RISC-V `virt` 机器上启动。
- 用户程序可以通过 `fork`、`exec`、`wait`、`exit` 运行。
- shell 支持 PATH 查找、行编辑、重定向、多级管道和 `$?`。
- 文件系统支持基础文件、目录、截断、删除、`stat` 和目录项复用，并有测试覆盖。
- `/bin/vi` 是当前主编辑器。
- MyOS 内置 `tcc`，可以在系统内编译简单 C 程序。
- 镜像里带有一套很小的 libc 和运行时对象。
- `make test-qemu` 可以自动启动 QEMU 跑回归测试。

它还不是完整 POSIX 系统。现在更准确地说，它是一个正在长大的实验性 OS，并且我们在逐步把每个关键行为变成可测试契约。

## 快速开始

宿主机通常需要这些工具：

- `make`
- `gcc`
- `python3`
- `qemu-system-riscv64`
- `riscv64-unknown-elf-gcc`、`ld`、`objcopy`、`objdump`
- `riscv64-linux-gnu-gcc`、`ar`

构建内核和文件系统镜像：

```sh
make kernel.elf fs.img
```

运行 MyOS：

```sh
make run
```

退出 QEMU：

```text
Ctrl-a x
```

运行自动回归测试：

```sh
make test-qemu
```

如果开发时 `fs.img` 变旧或损坏，可以重新生成：

```sh
rm -f fs.img
make fs.img
```

## 在 MyOS 里能做什么

基础 shell 工作流：

```sh
ls
ls /bin
pwd
cd /src
cd /
```

文件 IO 和命令组合：

```sh
echo hello > msg.txt
echo world >> msg.txt
cat < msg.txt
echo ok | cat | cat
echo $?
```

编辑、编译、运行 C 程序：

```sh
vi hello.c
tcc hello.c -o hello
./hello
echo $?
```

示例 `hello.c`：

```c
#include <stdio.h>

int main(void) {
    int x;
    scanf("%d", &x);
    printf("x=%d\n", x);
    return 0;
}
```

## 文件系统镜像内容

生成的 `fs.img` 当前会打包：

- `/bin/echo`
- `/bin/ls`
- `/bin/cat`
- `/bin/touch`
- `/bin/mkdir`
- `/bin/clear`
- `/bin/rm`
- `/bin/grep`
- `/bin/wc`
- `/bin/cp`
- `/bin/mv`
- `/bin/cmp`
- `/bin/head`
- `/bin/tail`
- `/bin/hexdump`
- `/bin/runtests`
- `/bin/sbrktest`
- `/bin/vi`
- `/bin/tcc`
- `/lib/crt1.o`、`/lib/crti.o`、`/lib/crtn.o`、`/lib/libc.a`
- `/include/...` MyOS 最小头文件集合
- `/src/tests/libc_ct.c`
- `/src/tests/fs_ct.c`
- `/src/tests/tty_ct.c`

`/bin/edit` 和 `/bin/kilo` 已经从镜像里移除。现在编辑器入口统一是 `/bin/vi`。

注意一个文件系统限制：单个路径组件长度受 `DIRSIZ = 14` 限制，所以镜像里的测试文件名使用 `libc_ct.c` 这种短名。

## Shell 契约

shell 源码在 [user/initcode.c](user/initcode.c)。它目前是第一个用户进程，也是主要交互环境。

当前支持：

- 通过 `/bin:/` 做 PATH 查找。
- 内建命令：`cd`、`pwd`、`PATH`、`export PATH=...`。
- Backspace 和左右方向键行编辑。
- 输出重定向：`>`。
- 追加重定向：`>>`。
- 输入重定向：`<`。
- 多级管道：`a | b | c`。
- 上一条命令状态：`$?`。
- Ctrl-C 可以终止前台用户程序，并保持 shell 存活。

当前退出状态约定：

- `0`：成功。
- `1`：普通命令失败或语法错误。
- `-1`：系统级失败、命令不存在、重定向打开失败或 Ctrl-C 终止。

这不是完整 POSIX shell 行为，而是当前 MyOS 明确承诺的 shell contract。

## 编辑器和 TTY

`vi` 是 neatvi 的移植版本，构建后放在 `/bin/vi`。

重要终端行为：

- `vi` 运行时会切换到 raw/no-echo 模式。
- `vi` 使用终端 alternate screen buffer，所以退出后不会把 `~`、`:wq` 等编辑器残影留在 shell 屏幕上。
- `tcgetattr` 和 `tcsetattr` 已经实现到足够支撑当前编辑器和测试。
- echo 行为目前仍有一部分在 libc 层模拟，还不是完整的内核 TTY line discipline。

这部分已经可用，但后续仍值得继续下沉到更系统的 kernel TTY 层。

## 工具链

镜像里包含移植后的 `tcc` 和最小运行时：

```text
/bin/tcc
/lib/crt1.o
/lib/crti.o
/lib/crtn.o
/lib/libc.a
/include
```

近期工具链目标是让 MyOS 内部能顺畅完成：

```text
写 C -> 编译 C -> 运行程序 -> 查看结果 -> 继续修改
```

长期目标是让更多用户态工具在 MyOS 内部被编译出来。

## 回归测试

主要测试入口：

```sh
make test-qemu
```

它会运行 [scripts/qemu_smoke.py](scripts/qemu_smoke.py)，启动 QEMU，复制一份临时 `fs.img`，驱动 shell，并检查系统行为。

当前覆盖：

- Shell：
  - `/bin` 内容
  - `>`、`>>`、`<`
  - 多级管道
  - `$?`
  - 语法错误
  - 重定向失败行为
  - Ctrl-C 后 shell 存活和退出状态
- `vi`：
  - 保存和退出
  - 保存短文件时正确截断旧内容
  - ESC 模式切换
  - 方向键
  - Backspace
  - alternate screen 退出行为
- `tcc`：
  - 在 MyOS 内编译 C 程序
  - 运行使用 `scanf` 的程序
  - 检查输入回显和输出结果
- libc contract：
  - `snprintf`、`sscanf`
  - `open(O_TRUNC)`
  - `fopen`、`fread`、`fwrite`
  - `lseek`
  - `malloc`、`realloc`、`free`
- 文件系统 contract：
  - 创建、读取、写入
  - `stat`、`fstat`
  - 截断
  - 删除后重建
  - 目录项复用
  - 独立 fd offset
- TTY contract：
  - 默认 `ECHO`、`ICANON`、`ISIG`
  - 关闭和恢复 echo
  - Ctrl-C 打断阻塞输入
  - `vi` 退出后恢复主屏幕且不清屏

修 bug 时，应该同步添加或更新 QEMU 回归测试。

## 仓库结构

```text
kernel/              内核代码：boot、trap、VM、proc、FS、driver、syscall
user/                用户程序和 init shell
ports/tinycc/        TCC 移植、MyOS libc、运行时对象、头文件
ports/neatvi/        vi 编辑器移植
tests/               会被打进 MyOS 并在系统内编译运行的 contract 测试
scripts/             宿主侧自动化脚本，包括 QEMU smoke test
mkfs.c               宿主侧 fs.img 构建工具
Makefile             构建、运行、测试入口
OUROBOROS.md         长期自举路线图
```

## 常用命令

构建运行所需文件：

```sh
make kernel.elf fs.img
```

运行：

```sh
make run
```

测试：

```sh
make test-qemu
```

清理生成文件：

```sh
make clean
```

只重建文件系统镜像：

```sh
rm -f fs.img
make fs.img
```

## 已知限制

- 没有 job control 和进程组。
- Ctrl-C 当前采用实用近似：杀掉非 shell 用户进程。
- shell 还没有引号、glob、通用变量和脚本。
- `termios` 仍是最小实现。
- 文件系统容量小，路径组件名短。
- MyOS 内部还没有 `make`。
- MyOS 还不能在自己内部重建完整 userland 或 kernel。

## 下一步方向

当前下一阶段是 **Toolsmith**：

- 添加小型 Unix 工具，例如 `wc`、`cp`、`mv`。
- 只在真实工具需要时继续补 libc。
- 每增加一个工具，都配 contract test。
- 为 MyOS 内部的小型构建工具做准备。

当前已经完成第一块工具箱砖：`/bin/grep`。它支持：

```sh
grep PATTERN FILE...
cat file | grep PATTERN
```

第二块工具箱砖是 `/bin/wc`。它支持：

```sh
wc FILE...
cat file | wc
```

第三块工具箱砖是 `/bin/cp`。它支持：

```sh
cp SRC DST
```

第四块工具箱砖是 `/bin/mv`。它支持：

```sh
mv SRC DST
```

第五块工具箱砖是 `/bin/cmp`。它支持：

```sh
cmp FILE1 FILE2
```

第六块工具箱砖是 `/bin/head` 和 `/bin/tail`。它们支持：

```sh
head [-n N] [FILE]
tail [-n N] [FILE]
cat file | head -n 5
cat file | tail -n 5
```

第七块工具箱砖是 `/bin/hexdump`。它支持：

```sh
hexdump [FILE]
cat file | hexdump
```

第一版 MyOS 内部 test runner 是 `/bin/runtests`。它会在系统内创建临时文件，运行一组轻量工具测试，收集退出状态和输出文件，并汇总：

```sh
runtests
runtests tools
runtests contracts
runtests list
runtests help
runtests wc
```

当前覆盖：

- `cp` + `cmp`
- `cmp` 不同文件
- `wc`
- `head`
- `tail`
- `hexdump`
- `mv`
- 编译并运行 `/src/tests/libc_ct.c`
- 编译并运行 `/src/tests/fs_ct.c`

`tty_ct.c` 仍由宿主机 QEMU smoke script 驱动，因为它需要交互输入、Ctrl-C 和终端模式检查。

`runtests` 不带参数等同于 `runtests all`。也可以运行 `runtests tools`、`runtests contracts`，或指定单项测试名，例如 `runtests wc`。`runtests list` 会列出当前所有测试名，`runtests help` 会输出用法。

下一步可以整理 `runtests` 的内部结构，让工具测试和 contract 测试更容易继续扩展；或者开始做一个很小的 build tool。当这些工作流在 MyOS 内部变得自然时，系统就可以开始走向第一轮用户态自举构建循环。
