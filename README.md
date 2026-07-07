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
- `/bin/install`
- `/bin/mv`
- `/bin/cmp`
- `/bin/diff`
- `/bin/head`
- `/bin/tail`
- `/bin/hexdump`
- `/bin/runtests`
- `/bin/build`
- `/bin/sbrktest`
- `/bin/cxxtest`
- `/bin/cxxstdtest`
- `/bin/cxxdyntest`
- `/bin/gxxtest`
- `/bin/vi`
- `/bin/tcc`
- `/lib/crt1.o`、`/lib/crti.o`、`/lib/crtn.o`、`/lib/libc.a`
- `/include/...` MyOS 最小头文件集合
- `/include/c++/...` MyOS freestanding C++ 头文件子集
- `/src/Buildfile`
- `/src/tests/libc_ct.c`
- `/src/tests/fs_ct.c`
- `/src/tests/tty_ct.c`
- `/src/tests/cxx_ct.cc`
- `/src/tests/cxxstd_ct.cc`
- `/src/tests/cxxdyn_ct.cc`
- `/src/tests/gxx_ct.cc`
- `/src/userland/cat.c`
- `/src/userland/diff.c`
- `/src/userland/grep.c`
- `/src/userland/hello.c`
- `/src/userland/lines.c`
- `/src/userland/wc.c`

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

## C++ contract

当前已经有三条标准 C++ 能力探针。宿主机使用 `riscv64-unknown-elf-g++` 交叉编译 [tests/cxx_contract.cc](tests/cxx_contract.cc)，并把产物打包成 `/bin/cxxtest`，源码打包成 `/src/tests/cxx_ct.cc`。

它不依赖 libstdc++，但使用真正的 C++ 前端，覆盖：

- 全局构造函数和 `.init_array`
- class / constructor / destructor
- function template 和 `constexpr`
- reference、overload
- virtual dispatch
- `new` / `delete`

这一步证明的是 MyOS 可以运行 C++ ABI 基础程序。MyOS 内部的 C++ 编译器还没有引入；当前系统内编译仍由 `/bin/tcc` 支撑 C 程序。

第二条探针是 [tests/cxx_std_contract.cc](tests/cxx_std_contract.cc)，产物是 `/bin/cxxstdtest`，源码打包成 `/src/tests/cxxstd_ct.cc`。它使用 MyOS 自带的 freestanding C++ 头文件子集 `/include/c++`，目前覆盖：

- `std::array`
- `std::pair` / `std::make_pair`
- `std::sort`
- `std::min` / `std::max`
- `std::is_same_v` / `std::remove_reference_t`

这不是完整 libstdc++，而是一组明确可审计、可逐步扩展的标准库子集头文件。

第三条探针是 [tests/cxx_dyn_contract.cc](tests/cxx_dyn_contract.cc)，产物是 `/bin/cxxdyntest`，源码打包成 `/src/tests/cxxdyn_ct.cc`。它把 C++ 子集推进到动态内存容器：

- `std::vector<T>`
- `std::string`
- 对象拷贝、移动、析构
- `vector<string>` 与 `std::sort`
- `std::nothrow` new/delete 与可复用 C++ heap

当前 `vector` 和 `string` 是最小可用实现，用于推进 MyOS 的 C++ 运行能力，不等同于完整标准库实现。

## GCC 适配目标

当前语言工具链优先级已经调整为 **GCC C 前端先行，g++ 后置**。原因是 GCC C 能先暴露 MyOS 作为编译器宿主系统的真实缺口：文件系统容量、路径、临时文件、`stat/access/rename/getcwd`、进程模型、stdio 和内存管理。

第一步已经有宿主侧 MyOS C sysroot 入口：

```sh
make gcc-sysroot
build/riscv64-myos-gcc tests/libc_contract.c -o build/gcc-libc-contract
```

`build/riscv64-myos-gcc` 是一个薄 wrapper，使用宿主机 `riscv64-unknown-elf-gcc`，自动带上 MyOS 的 `crt1.o`、`crti.o`、`crtn.o`、`libc.a` 和 `/include`。这一步解决的是“宿主机 GCC 正式面向 MyOS target 出 C 程序”，不是“gcc 编译器本体已经能在 MyOS 内运行”。

这个 wrapper 编译出的 C contract 已经打包为 `/bin/gcctest` 并纳入常规 QEMU smoke。当前也会用同一入口编译一组宿主 GCC 产出的 userland 工具，并打包为：

- `/bin/gcc-hello`
- `/bin/gcc-lines`
- `/bin/gcc-cat`
- `/bin/gcc-wc`
- `/bin/gcc-grep`
- `/bin/gcc-diff`

它们覆盖了 printf、文件读取、stdin 管道、字符串处理和 basic line diff。后续目标是继续扩大到：

- `riscv64-myos-gcc hello.c -o hello`
- `riscv64-myos-gcc userland/*.c`
- 在 MyOS 内运行 gcc C 前端本体
- 在 MyOS 内完成 `gcc hello.c -o hello && ./hello`

为 native GCC 做准备的 libc/FS 缺口会逐项进入 contract。当前已经补入 `rename(old, new)`，覆盖普通文件重命名、覆盖已有文件和跨目录重命名；这对应编译器常见的“先写临时文件，再 rename 成最终输出”的模式。

## C++ 适配目标

C++ 当前不作为 native 编译器移植的第一优先级。它的目标是先形成一条稳定、可测试、可逐步扩展的 freestanding C++ 路线，为后续 g++ 回归做准备：

- MyOS 能运行由宿主机 `riscv64-unknown-elf-g++` 生成的 C++ ELF。
- C++ ABI 基础可用：全局构造函数、虚函数、析构、`new/delete`。
- `/include/c++` 提供小而明确的标准库子集，优先覆盖内核工具和用户态工具真正会用到的头文件。
- C++ heap 能支撑 `vector`、`string` 这类动态容器反复构造和析构，而不是只靠一次性 `sbrk` 增长。
- 每新增一个 C++ 能力，都要落到 `/bin/cxxtest`、`/bin/cxxstdtest`、`/bin/cxxdyntest` 或 `/bin/gxxtest` 这类 contract 里。

当前也有宿主侧 MyOS C++ sysroot 入口：

```sh
make cxx-sysroot
build/riscv64-myos-g++ tests/cxx_dyn_contract.cc -o build/cxxdyntest2
```

`build/riscv64-myos-g++` 是一个薄 wrapper，使用宿主机 `riscv64-unknown-elf-g++`，自动带上 MyOS 的 `crt0`、`libcxxrt.a`、`libc.a`、C libc headers 和 `/include/c++` 子集。这一步解决的是“宿主机 g++ 正式面向 MyOS target 出程序”，不是“g++ 编译器本体已经能在 MyOS 内运行”。

当前新增的 [tests/gxx_contract.cc](tests/gxx_contract.cc) 会通过这个 wrapper 编译为 `/bin/gxxtest`，覆盖 C++ 标准库子集和 C libc 同时参与链接的情况：

- 全局构造函数
- `std::vector<std::string>`
- `std::sort`
- `<stdio.h>` / `<string.h>` 中的 `snprintf`、`strcmp`

做到这些之后，MyOS 就具备了承接更大语言运行时的基础。下一条自然路线是尝试 Python：先不是完整 CPython，而是先找出解释器需要的系统调用、libc、文件 IO、内存分配和构建能力缺口，再用 contract 一项项补齐。

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
- C++ / g++ contract：
  - C++ ABI 基础程序
  - freestanding `/include/c++` 子集
  - `std::vector`、`std::string` 和 C++ heap
  - g++ wrapper 输出的 MyOS ELF 能同时使用 C++ runtime 和 C libc

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

当前下一阶段是 **Forge + 语言运行时探索**：

- 继续扩大 MyOS 内部可构建的 userland 工具集合。
- 继续收紧 C++ freestanding runtime 和 `/include/c++` 子集。
- 评估 Python 移植路径，优先验证最小解释器运行所需的 libc、FS、TTY、进程和内存契约。
- 每增加一个工具或语言运行时能力，都配 contract test。
- 为 MyOS 内部的小型构建工具和后续包管理做准备。

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

第四块工具箱砖是 `/bin/install`。它支持：

```sh
install SRC DST
```

它目前是一个明确安装语义的文件复制工具，用于把构建产物放进 `/bin` 这类系统路径。

第五块工具箱砖是 `/bin/mv`。它支持：

```sh
mv SRC DST
```

第六块工具箱砖是 `/bin/cmp`。它支持：

```sh
cmp FILE1 FILE2
```

第七块工具箱砖是 `/bin/diff`。它支持：

```sh
diff FILE1 FILE2
```

当前是 minimal line diff：相同文件静默返回 0，不同文件输出第一处不同的行并返回 1。

第八块工具箱砖是 `/bin/head` 和 `/bin/tail`。它们支持：

```sh
head [-n N] [FILE]
tail [-n N] [FILE]
cat file | head -n 5
cat file | tail -n 5
```

第九块工具箱砖是 `/bin/hexdump`。它支持：

```sh
hexdump [FILE]
cat file | hexdump
```

`/src/userland/cat.c`、`/src/userland/diff.c`、`/src/userland/wc.c` 和 `/src/userland/grep.c` 是第一批由 MyOS 内部工具链重建已有基础工具的源码。运行：

```sh
build userland
build install
/bin/cat2 file.txt
cat file.txt | /bin/cat2
/bin/diff2 left.txt right.txt
/bin/wc2 file.txt
cat file.txt | /bin/wc2
/bin/grep2 PATTERN file.txt
cat file.txt | /bin/grep2 PATTERN
```

可以得到通过 MyOS 内部 `tcc` 构建并安装的 `cat2`、`diff2`、`wc2` 和 `grep2`。

第一版 MyOS 内部 test runner 是 `/bin/runtests`。它会在系统内创建临时文件，运行一组轻量工具测试，收集退出状态和输出文件，并汇总：

```sh
runtests
runtests tools
runtests contracts
runtests world
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
- `diff`
- `mv`
- 编译并运行 `/src/tests/libc_ct.c`
- 编译并运行 `/src/tests/fs_ct.c`
- `build world` 后验证 self-built `/bin/cat2`、`/bin/diff2`、`/bin/wc2`、`/bin/grep2`、`/bin/lines`、`/bin/hello`

`tty_ct.c` 仍由宿主机 QEMU smoke script 驱动，因为它需要交互输入、Ctrl-C 和终端模式检查。

`runtests` 不带参数等同于 `runtests all`。默认 `all` 保持轻量，会运行 tools 和 contracts；`world` 需要显式运行：

```sh
build world
runtests world
```

也可以运行 `runtests tools`、`runtests contracts`，或指定单项测试名，例如 `runtests wc`。`runtests list` 会列出当前所有测试名，`runtests help` 会输出用法。

第一版构建工具是 `/bin/build`。它从 `/src/Buildfile` 读取构建目标，当前支持：

```sh
build list
build help
build libc-contract
build fs-contract
build contracts
build userland
build install
build world
```

`/src/Buildfile` 目前使用六列格式：

```text
TARGET RECIPE SOURCE OUTPUT GROUP DEPS
```

当前支持的 `RECIPE` 有三个：

- `cc`：等价于运行 `/bin/tcc SOURCE -o OUTPUT`
- `copy`：把 `SOURCE` 复制到 `OUTPUT`
- `phony`：不产生文件，只用于聚合依赖目标

`DEPS` 是逗号分隔的依赖列表。普通项表示文件路径，`@TARGET` 表示另一个构建目标；例如 `@wc2` 会在当前目标执行前先构建 `wc2`。输出文件已经存在且非空时，`build` 会跳过该目标。旧的五列格式仍可读取，并会把 `SOURCE` 当作默认依赖。

例如：

```text
libc-contract cc /src/tests/libc_ct.c rt_libc contracts /src/tests/libc_ct.c
fs-contract cc /src/tests/fs_ct.c rt_fs contracts /src/tests/fs_ct.c
cat2 cc /src/userland/cat.c cat2 userland /src/userland/cat.c
diff2 cc /src/userland/diff.c diff2 userland /src/userland/diff.c
grep2 cc /src/userland/grep.c grep2 userland /src/userland/grep.c
hello cc /src/userland/hello.c hello userland /src/userland/hello.c
lines cc /src/userland/lines.c lines userland /src/userland/lines.c
wc2 cc /src/userland/wc.c wc2 userland /src/userland/wc.c
cat2-install copy cat2 /bin/cat2 install @cat2
diff2-install copy diff2 /bin/diff2 install @diff2
grep2-install copy grep2 /bin/grep2 install @grep2
hello-install copy hello /bin/hello install @hello
lines-install copy lines /bin/lines install @lines
wc2-install copy wc2 /bin/wc2 install @wc2
world phony - - world @libc-contract,@fs-contract,@cat2-install,@diff2-install,@grep2-install,@hello-install,@lines-install,@wc2-install
```

`runtests contracts` 会通过 `build` 编译 contract，再运行并判定结果。这样 `build` 负责构建，`runtests` 负责测试判定。

`build userland && build install` 会在 MyOS 内部编译 `/src/userland/cat.c`、`/src/userland/diff.c`、`/src/userland/grep.c`、`/src/userland/hello.c`、`/src/userland/lines.c` 和 `/src/userland/wc.c`，再安装为 `/bin/cat2`、`/bin/diff2`、`/bin/grep2`、`/bin/hello`、`/bin/lines` 和 `/bin/wc2`。由于 install 目标使用 `@TARGET` 依赖，直接运行 `build install` 也会自动先构建缺失的本地产物。`lines` 是一个简化的行计数工具，支持文件和 stdin：

`build world` 是当前的整体构建入口，会编译 contract、构建 self-built userland，并安装这些工具。

```sh
/bin/lines file.txt
cat file.txt | /bin/lines
```

这是一条最小但可用的 userland build/install 闭环。

也可以直接使用 `/bin/install` 安装某个构建产物：

```sh
install lines /bin/lines2
/bin/lines2 file.txt
```

下一步可以把更多用户态工具源码放进 `/src/userland`，或给 `Buildfile` 增加多命令 recipe、时间戳判断。当这些工作流在 MyOS 内部变得自然时，系统就可以开始走向第一轮用户态自举构建循环。
