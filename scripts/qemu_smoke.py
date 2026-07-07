#!/usr/bin/env python3
import argparse
import os
import re
import select
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import tty


PROMPT = b"MyOS:/>"


class TestFailure(Exception):
    pass


def strip_ansi(data):
    data = re.sub(rb"\x1b\[[0-9;?]*[ -/]*[@-~]", b"", data)
    data = data.replace(b"\r", b"")
    return data


def show(data, limit=3000):
    text = strip_ansi(data).decode("utf-8", "replace")
    if len(text) > limit:
        text = text[-limit:]
    return text


class Qemu:
    def __init__(self, qemu, kernel, image, timeout):
        self.timeout = timeout
        self.master = None
        self.proc = None
        master, slave = os.openpty()
        tty.setraw(slave)
        args = [
            qemu,
            "-machine", "virt",
            "-bios", "none",
            "-kernel", kernel,
            "-drive", f"file={image},if=none,format=raw,id=x0",
            "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0",
            "-nographic",
        ]
        self.proc = subprocess.Popen(
            args,
            stdin=slave,
            stdout=slave,
            stderr=slave,
            close_fds=True,
            start_new_session=True,
        )
        os.close(slave)
        self.master = master

    def close(self):
        if self.master is not None:
            try:
                os.write(self.master, b"\x01x")
            except OSError:
                pass
        if self.proc is not None:
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(self.proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    self.proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    try:
                        os.killpg(self.proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
        if self.master is not None:
            try:
                os.close(self.master)
            except OSError:
                pass
            self.master = None

    def read_for(self, seconds):
        end = time.monotonic() + seconds
        out = bytearray()
        while time.monotonic() < end:
            timeout = max(0, min(0.05, end - time.monotonic()))
            ready, _, _ = select.select([self.master], [], [], timeout)
            if not ready:
                continue
            try:
                chunk = os.read(self.master, 4096)
            except OSError:
                break
            if not chunk:
                break
            out.extend(chunk)
        return bytes(out)

    def read_until(self, needle, timeout=None, label="output"):
        if timeout is None:
            timeout = self.timeout
        end = time.monotonic() + timeout
        out = bytearray()
        while time.monotonic() < end:
            ready, _, _ = select.select([self.master], [], [], 0.05)
            if not ready:
                if self.proc.poll() is not None:
                    raise TestFailure(f"QEMU exited while waiting for {label}\n{show(out)}")
                continue
            try:
                chunk = os.read(self.master, 4096)
            except OSError:
                break
            if not chunk:
                break
            out.extend(chunk)
            if needle in out:
                return bytes(out)
        raise TestFailure(f"timeout waiting for {label}\n{show(out)}")

    def send(self, data):
        if isinstance(data, str):
            data = data.encode("utf-8")
        os.write(self.master, data)

    def command(self, command, timeout=None):
        self.send(command + "\r")
        return self.read_until(PROMPT, timeout=timeout, label=f"prompt after {command!r}")

    def open_vi(self, path):
        self.send(f"vi {path}\r")
        self.read_for(0.4)

    def vi(self, path, keys, timeout=None):
        self.open_vi(path)
        self.send(keys)
        return self.read_until(PROMPT, timeout=timeout, label=f"vi {path!r} exit")


def require(data, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    if needle not in strip_ansi(data):
        raise TestFailure(f"missing {label!r}\n{show(data)}")


def forbid(data, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    if needle in strip_ansi(data):
        raise TestFailure(f"unexpected {label!r}\n{show(data)}")


def slice_between(data, start, end, label):
    if isinstance(start, str):
        start = start.encode("utf-8")
    if isinstance(end, str):
        end = end.encode("utf-8")
    clean = strip_ansi(data)
    start_pos = clean.find(start)
    if start_pos < 0:
        raise TestFailure(f"missing start marker for {label!r}\n{show(data)}")
    start_pos += len(start)
    end_pos = clean.find(end, start_pos)
    if end_pos < 0:
        raise TestFailure(f"missing end marker for {label!r}\n{show(data)}")
    return clean[start_pos:end_pos]


def require_between(data, start, end, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    segment = slice_between(data, start, end, label)
    if needle not in segment:
        raise TestFailure(f"missing {label!r}\n{show(segment)}")


def forbid_between(data, start, end, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    segment = slice_between(data, start, end, label)
    if needle in segment:
        raise TestFailure(f"unexpected {label!r}\n{show(segment)}")


def forbid_raw(data, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    if needle in data:
        raise TestFailure(f"unexpected raw {label!r}\n{show(data)}")


def require_raw(data, needle, label):
    if isinstance(needle, str):
        needle = needle.encode("utf-8")
    if needle not in data:
        raise TestFailure(f"missing raw {label!r}\n{show(data)}")


def cleanup(q, *names):
    if names:
        q.command("rm " + " ".join(names), timeout=5)


def test_shell(q):
    cleanup(
        q,
        "redir.txt",
        "append.txt",
        "input.txt",
        "pipeout.txt",
        "missing.txt",
        "shouldnot",
    )
    out = q.command("ls /bin")
    require(out, "vi", "vi editor in /bin")
    require(out, "grep", "grep tool in /bin")
    require(out, "install", "install tool in /bin")
    forbid(out, "edit", "removed edit alias in /bin")
    forbid(out, "kilo", "removed kilo editor in /bin")

    q.command("echo long-long-line > redir.txt")
    q.command("echo x > redir.txt")
    out = q.command("cat redir.txt")
    require(out, "\nx\n", "truncated redirection output")
    forbid(out, "long-long-line", "stale redirected file tail")

    q.command("echo a > append.txt")
    q.command("echo b >> append.txt")
    q.command("echo c>>append.txt")
    out = q.command("cat append.txt")
    require(out, "\na\nb\nc\n", "append redirection output")

    q.command("echo from-file > input.txt")
    out = q.command("cat < input.txt")
    require(out, "from-file", "input redirection output")

    out = q.command("echo pipe-ok | cat")
    require(out, "pipe-ok", "pipeline output")

    out = q.command("echo pipe-v2 | cat | cat")
    require(out, "pipe-v2", "multi-stage pipeline output")

    q.command("echo pipe-file | cat | cat > pipeout.txt")
    out = q.command("cat pipeout.txt")
    require(out, "pipe-file", "pipeline output redirection")

    q.command("echo ok | cat")
    out = q.command("echo $?")
    require(out, "\n0\n", "successful pipeline exit status")

    q.command("echo ok | rm")
    out = q.command("echo $?")
    require(out, "\n1\n", "pipeline uses last command exit status")

    q.command("rm")
    out = q.command("echo $?")
    require(out, "\n1\n", "last command exit status")

    q.command("nosuchcmd")
    out = q.command("echo $?")
    require(out, "\n-1\n", "command-not-found exit status")

    q.command("cd missing_dir")
    out = q.command("echo $?")
    require(out, "\n1\n", "failed cd exit status")
    q.command("cd /")
    out = q.command("echo $?")
    require(out, "\n0\n", "successful cd exit status")

    q.command("echo bad >")
    out = q.command("echo $?")
    require(out, "\n1\n", "redirection syntax error status")

    q.command("echo bad |")
    out = q.command("echo $?")
    require(out, "\n1\n", "pipe syntax error status")

    q.command("cat < missing.txt > shouldnot")
    out = q.command("echo $?")
    require(out, "\n-1\n", "input redirection failure status")
    out = q.command("cat shouldnot")
    require(out, "cat: cannot open shouldnot", "failed input redirection does not create output")

    q.send("cat\r")
    q.read_for(0.2)
    q.send("cat-input\r")
    out = q.read_for(0.5)
    require(out, "cat-input", "cat stdin echo before Ctrl-C")
    q.send(b"\x03")
    out = q.read_until(PROMPT, timeout=8, label="prompt after Ctrl-C")
    require(out, PROMPT, "prompt after Ctrl-C")
    out = q.command("echo $?")
    require(out, "\n-1\n", "Ctrl-C exit status")


def test_grep_tool(q):
    cleanup(q, "g1.txt", "g2.txt")

    q.command("echo alpha > g1.txt")
    q.command("echo beta >> g1.txt")
    q.command("echo gamma >> g1.txt")
    q.command("echo beta-two > g2.txt")

    out = q.command("grep beta g1.txt")
    require(out, "beta", "grep file match")
    forbid(out, "alpha", "grep excludes nonmatching line")
    out = q.command("echo $?")
    require(out, "\n0\n", "grep match exit status")

    out = q.command("cat g1.txt | grep gamma")
    require(out, "gamma", "grep stdin pipeline match")
    out = q.command("echo $?")
    require(out, "\n0\n", "grep pipeline match exit status")

    out = q.command("grep beta g1.txt g2.txt")
    require(out, "g1.txt:beta", "grep multi-file first prefix")
    require(out, "g2.txt:beta-two", "grep multi-file second prefix")

    q.command("grep absent g1.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "grep no-match exit status")

    q.command("grep beta missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "grep missing-file exit status")


def test_wc_tool(q):
    cleanup(q, "wc1.txt", "wc2.txt", "empty.txt")

    q.command("echo alpha beta > wc1.txt")
    q.command("echo gamma >> wc1.txt")
    q.command("echo z > wc2.txt")
    q.command("touch empty.txt")

    out = q.command("wc wc1.txt")
    require(out, "2 3 17 wc1.txt", "wc single file counts")

    out = q.command("cat wc1.txt | wc")
    require(out, "2 3 17", "wc stdin pipeline counts")

    out = q.command("wc empty.txt")
    require(out, "0 0 0 empty.txt", "wc empty file counts")

    out = q.command("wc wc1.txt wc2.txt")
    require(out, "2 3 17 wc1.txt", "wc multi-file first counts")
    require(out, "1 1 2 wc2.txt", "wc multi-file second counts")
    require(out, "3 4 19 total", "wc multi-file total counts")

    q.command("wc missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "wc missing-file exit status")


def test_cp_tool(q):
    cleanup(q, "cp_src.txt", "cp_dst.txt", "cp_short.txt")

    q.command("echo alpha beta > cp_src.txt")
    q.command("echo gamma >> cp_src.txt")
    out = q.command("cp cp_src.txt cp_dst.txt")
    forbid(out, "cp:", "cp successful copy has no error")
    out = q.command("cat cp_dst.txt")
    require(out, "alpha beta", "cp copied first line")
    require(out, "gamma", "cp copied appended line")
    out = q.command("echo $?")
    require(out, "\n0\n", "cp success exit status")

    q.command("echo very-long-stale-tail > cp_dst.txt")
    q.command("echo z > cp_short.txt")
    q.command("cp cp_short.txt cp_dst.txt")
    out = q.command("cat cp_dst.txt")
    require(out, "\nz\n", "cp overwrote destination")
    forbid(out, "very-long-stale-tail", "cp truncates destination")

    q.command("cp missing.txt cp_dst.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "cp missing-source exit status")

    q.command("cp cp_src.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "cp usage exit status")


def test_install_tool(q):
    cleanup(q, "inst_src.txt", "inst_dst.txt", "inst_s.txt", "lines2")

    q.command("echo alpha > inst_src.txt")
    q.command("echo beta >> inst_src.txt")
    out = q.command("install inst_src.txt inst_dst.txt")
    forbid(out, "install:", "install successful copy has no error")
    out = q.command("cat inst_dst.txt")
    require(out, "alpha", "install copied first line")
    require(out, "beta", "install copied second line")
    out = q.command("echo $?")
    require(out, "\n0\n", "install success exit status")

    q.command("echo very-long-stale-tail > inst_dst.txt")
    q.command("echo z > inst_s.txt")
    q.command("install inst_s.txt inst_dst.txt")
    out = q.command("cat inst_dst.txt")
    require(out, "\nz\n", "install overwrote destination")
    forbid(out, "very-long-stale-tail", "install truncates destination")

    q.command("install missing.txt inst_dst.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "install missing-source exit status")

    q.command("install inst_src.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "install usage exit status")


def test_mv_tool(q):
    cleanup(q, "mv_src.txt", "mv_dst.txt", "mv_same.txt", "mv_old.txt", "mv_short.txt")

    q.command("echo alpha beta > mv_src.txt")
    q.command("echo gamma >> mv_src.txt")
    out = q.command("mv mv_src.txt mv_dst.txt")
    forbid(out, "mv:", "mv successful move has no error")
    out = q.command("cat mv_dst.txt")
    require(out, "alpha beta", "mv copied first line")
    require(out, "gamma", "mv copied appended line")
    out = q.command("cat mv_src.txt")
    require(out, "cat: cannot open mv_src.txt", "mv removed source")
    out = q.command("echo $?")
    require(out, "\n0\n", "mv success exit status")

    q.command("echo very-long-stale-tail > mv_dst.txt")
    q.command("echo z > mv_short.txt")
    q.command("mv mv_short.txt mv_dst.txt")
    out = q.command("cat mv_dst.txt")
    require(out, "\nz\n", "mv overwrote destination")
    forbid(out, "very-long-stale-tail", "mv truncates destination")
    out = q.command("cat mv_short.txt")
    require(out, "cat: cannot open mv_short.txt", "mv removed overwritten source")

    q.command("echo keep > mv_same.txt")
    q.command("mv mv_same.txt mv_same.txt")
    out = q.command("cat mv_same.txt")
    require(out, "keep", "mv same-file preserves content")

    q.command("mv missing.txt mv_dst.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "mv missing-source exit status")

    q.command("mv mv_dst.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "mv usage exit status")


def test_cmp_tool(q):
    cleanup(q, "cmp_a.txt", "cmp_b.txt", "cmp_c.txt", "cmp_d.txt")

    q.command("echo alpha > cmp_a.txt")
    q.command("echo alpha > cmp_b.txt")
    out = q.command("cmp cmp_a.txt cmp_b.txt")
    forbid(out, "differ", "cmp identical files are quiet")
    out = q.command("echo $?")
    require(out, "\n0\n", "cmp identical exit status")

    q.command("echo alXha > cmp_c.txt")
    out = q.command("cmp cmp_a.txt cmp_c.txt")
    require(out, "cmp_a.txt cmp_c.txt differ: byte 3", "cmp differing byte output")
    out = q.command("echo $?")
    require(out, "\n1\n", "cmp differing exit status")

    q.command("echo alphabet > cmp_d.txt")
    out = q.command("cmp cmp_a.txt cmp_d.txt")
    require(out, "cmp_a.txt cmp_d.txt differ: byte 6", "cmp differing length output")
    out = q.command("echo $?")
    require(out, "\n1\n", "cmp differing length exit status")

    q.command("cmp cmp_a.txt missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "cmp missing-file exit status")

    q.command("cmp cmp_a.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "cmp usage exit status")


def test_diff_tool(q):
    cleanup(q, "diff_a.txt", "diff_b.txt", "diff_c.txt", "diff_d.txt")

    q.command("echo alpha > diff_a.txt")
    q.command("echo beta >> diff_a.txt")
    q.command("echo alpha > diff_b.txt")
    q.command("echo beta >> diff_b.txt")
    out = q.command("diff diff_a.txt diff_b.txt")
    forbid(out, "differ", "diff identical files are quiet")
    out = q.command("echo $?")
    require(out, "\n0\n", "diff identical exit status")

    q.command("echo alpha > diff_c.txt")
    q.command("echo zeta >> diff_c.txt")
    out = q.command("diff diff_a.txt diff_c.txt")
    require(out, "diff_a.txt diff_c.txt differ: line 2", "diff differing line header")
    require(out, "- beta", "diff removed line")
    require(out, "+ zeta", "diff added line")
    out = q.command("echo $?")
    require(out, "\n1\n", "diff differing exit status")

    q.command("echo alpha > diff_d.txt")
    out = q.command("diff diff_a.txt diff_d.txt")
    require(out, "diff_a.txt diff_d.txt differ: line 2", "diff EOF line header")
    require(out, "- beta", "diff EOF removed line")
    require(out, "+ <EOF>", "diff EOF marker")
    out = q.command("echo $?")
    require(out, "\n1\n", "diff EOF exit status")

    q.command("diff diff_a.txt missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "diff missing-file exit status")

    q.command("diff diff_a.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "diff usage exit status")


def test_head_tail_tools(q):
    cleanup(q, "hd.txt", "tl.txt")

    q.command("echo h1 > hd.txt")
    for i in range(2, 13):
        q.command(f"echo h{i} >> hd.txt")
    q.command("cp hd.txt tl.txt")

    out = q.command("head -n 3 hd.txt")
    require(out, "h1", "head -n first line")
    require(out, "h2", "head -n second line")
    require(out, "h3", "head -n third line")
    forbid(out, "h4", "head -n stops after requested lines")

    out = q.command("head hd.txt")
    require(out, "h10", "head default tenth line")
    forbid(out, "h11", "head default stops at ten lines")

    out = q.command("cat hd.txt | head -n 2")
    require(out, "h1", "head stdin first line")
    require(out, "h2", "head stdin second line")
    forbid(out, "h3", "head stdin stops after requested lines")

    q.command("head missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "head missing-file exit status")

    q.command("head -n bad hd.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "head bad-count exit status")

    out = q.command("tail -n 3 tl.txt")
    require(out, "h10", "tail -n first retained line")
    require(out, "h11", "tail -n second retained line")
    require(out, "h12", "tail -n third retained line")
    forbid(out, "h9", "tail -n keeps only requested lines")

    out = q.command("tail tl.txt")
    require(out, "h3", "tail default first retained line")
    require(out, "h12", "tail default last line")
    forbid(out, "h2", "tail default keeps ten lines")

    out = q.command("cat tl.txt | tail -n 2")
    require(out, "h11", "tail stdin first retained line")
    require(out, "h12", "tail stdin second retained line")
    forbid(out, "h10", "tail stdin keeps only requested lines")

    q.command("tail missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "tail missing-file exit status")

    q.command("tail -n bad tl.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "tail bad-count exit status")


def test_hexdump_tool(q):
    cleanup(q, "hex.txt")

    q.command("echo hi > hex.txt")
    out = q.command("hexdump hex.txt")
    require(out, "00000000", "hexdump file offset")
    require(out, "68 69 0a", "hexdump file bytes")
    require(out, "|hi.|", "hexdump file ascii")

    out = q.command("echo AZ | hexdump")
    require(out, "41 5a 0a", "hexdump stdin bytes")
    require(out, "|AZ.|", "hexdump stdin ascii")

    q.command("hexdump missing.txt")
    out = q.command("echo $?")
    require(out, "\n-1\n", "hexdump missing-file exit status")

    q.command("hexdump hex.txt extra")
    out = q.command("echo $?")
    require(out, "\n1\n", "hexdump usage exit status")


def test_build_tool(q):
    cleanup(
        q,
        "rt_libc",
        "rt_fs",
        "build.log",
        "ct_trunc",
        "ct_stdio",
        "ct_seek",
        "fs_basic",
        "fs_trunc",
        "fs_unlink",
        "fs_seek",
        "fs_offset",
        "cat2",
        "cat2.txt",
        "diff2",
        "diff2a.txt",
        "diff2b.txt",
        "grep2",
        "grep2a.txt",
        "grep2b.txt",
        "hello",
        "lines",
        "lines.txt",
        "wc2",
        "wc2a.txt",
        "wc2b.txt",
        "/bin/cat2",
        "/bin/diff2",
        "/bin/grep2",
        "/bin/hello",
        "/bin/lines",
        "/bin/wc2",
    )

    out = q.command("cat /src/Buildfile", timeout=5)
    require(out, "libc-contract cc /src/tests/libc_ct.c rt_libc contracts /src/tests/libc_ct.c", "build file libc target")
    require(out, "fs-contract cc /src/tests/fs_ct.c rt_fs contracts /src/tests/fs_ct.c", "build file fs target")
    require(out, "cat2 cc /src/userland/cat.c cat2 userland /src/userland/cat.c", "build file cat2 target")
    require(out, "diff2 cc /src/userland/diff.c diff2 userland /src/userland/diff.c", "build file diff2 target")
    require(out, "grep2 cc /src/userland/grep.c grep2 userland /src/userland/grep.c", "build file grep2 target")
    require(out, "hello cc /src/userland/hello.c hello userland /src/userland/hello.c", "build file userland target")
    require(out, "lines cc /src/userland/lines.c lines userland /src/userland/lines.c", "build file lines target")
    require(out, "wc2 cc /src/userland/wc.c wc2 userland /src/userland/wc.c", "build file wc2 target")
    require(out, "cat2-install copy cat2 /bin/cat2 install @cat2", "build file cat2 install target")
    require(out, "diff2-install copy diff2 /bin/diff2 install @diff2", "build file diff2 install target")
    require(out, "grep2-install copy grep2 /bin/grep2 install @grep2", "build file grep2 install target")
    require(out, "hello-install copy hello /bin/hello install @hello", "build file install target")
    require(out, "lines-install copy lines /bin/lines install @lines", "build file lines install target")
    require(out, "wc2-install copy wc2 /bin/wc2 install @wc2", "build file wc2 install target")
    require(out, "world phony - - world @libc-contract,@fs-contract,@cat2-install,@diff2-install,@grep2-install,@hello-install,@lines-install,@wc2-install", "build file world target")
    out = q.command("cat /src/userland/cat.c", timeout=5)
    require(out, "cat2: cannot open", "userland cat2 source")
    out = q.command("cat /src/userland/diff.c", timeout=5)
    require(out, "diff2: cannot open", "userland diff2 source")
    out = q.command("cat /src/userland/grep.c", timeout=5)
    require(out, "grep2: cannot open", "userland grep2 source")
    out = q.command("cat /src/userland/hello.c", timeout=5)
    require(out, "HELLO_USERLAND", "userland hello source")
    out = q.command("cat /src/userland/lines.c", timeout=5)
    require(out, "count_fd", "userland lines source")
    out = q.command("cat /src/userland/wc.c", timeout=5)
    require(out, "wc2: cannot open", "userland wc2 source")

    out = q.command("build list", timeout=5)
    require(out, "contracts:", "build list contracts group")
    require(out, "libc-contract", "build list libc target")
    require(out, "fs-contract", "build list fs target")
    require(out, "userland:", "build list userland group")
    require(out, "cat2", "build list cat2 target")
    require(out, "diff2", "build list diff2 target")
    require(out, "grep2", "build list grep2 target")
    require(out, "hello", "build list hello target")
    require(out, "lines", "build list lines target")
    require(out, "wc2", "build list wc2 target")
    require(out, "install:", "build list install group")
    require(out, "cat2-install", "build list cat2 install target")
    require(out, "diff2-install", "build list diff2 install target")
    require(out, "grep2-install", "build list grep2 install target")
    require(out, "hello-install", "build list hello install target")
    require(out, "lines-install", "build list lines install target")
    require(out, "wc2-install", "build list wc2 install target")
    require(out, "world:", "build list world group")
    require(out, "world", "build list world target")

    out = q.command("build help", timeout=5)
    require(out, "usage: build", "build help usage")

    out = q.command("build libc-contract", timeout=30)
    require(out, "BUILD_PASS libc-contract", "build libc contract pass")
    out = q.command("./rt_libc", timeout=10)
    require(out, "LIBC_CONTRACT_PASS", "built libc contract runs")

    out = q.command("build libc-contract", timeout=10)
    require(out, "BUILD_SKIP libc-contract", "build libc contract skip")

    out = q.command("build contracts", timeout=60)
    require(out, "BUILD_SKIP libc-contract", "build contracts libc skip")
    require(out, "BUILD_PASS fs-contract", "build contracts fs pass")

    out = q.command("build contracts", timeout=10)
    require(out, "BUILD_SKIP libc-contract", "build contracts libc second skip")
    require(out, "BUILD_SKIP fs-contract", "build contracts fs skip")

    out = q.command("build userland", timeout=30)
    require(out, "BUILD_PASS cat2", "build userland cat2 pass")
    require(out, "BUILD_PASS diff2", "build userland diff2 pass")
    require(out, "BUILD_PASS grep2", "build userland grep2 pass")
    require(out, "BUILD_PASS hello", "build userland hello pass")
    require(out, "BUILD_PASS lines", "build userland lines pass")
    require(out, "BUILD_PASS wc2", "build userland wc2 pass")
    q.command("echo alpha > cat2.txt")
    q.command("echo beta >> cat2.txt")
    out = q.command("./cat2 cat2.txt", timeout=10)
    require(out, "alpha", "built cat2 reads file first line")
    require(out, "beta", "built cat2 reads file second line")
    out = q.command("cat cat2.txt | ./cat2", timeout=10)
    require(out, "alpha", "built cat2 reads stdin first line")
    require(out, "beta", "built cat2 reads stdin second line")
    q.command("./cat2 missing.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "built cat2 missing-file exit status")
    q.command("echo alpha beta > wc2a.txt")
    q.command("echo gamma >> wc2a.txt")
    q.command("echo z > wc2b.txt")
    out = q.command("./wc2 wc2a.txt", timeout=10)
    require(out, "2 3 17 wc2a.txt", "built wc2 single file counts")
    out = q.command("cat wc2a.txt | ./wc2", timeout=10)
    require(out, "2 3 17", "built wc2 stdin counts")
    out = q.command("./wc2 wc2a.txt wc2b.txt", timeout=10)
    require(out, "2 3 17 wc2a.txt", "built wc2 multi first counts")
    require(out, "1 1 2 wc2b.txt", "built wc2 multi second counts")
    require(out, "3 4 19 total", "built wc2 total counts")
    q.command("./wc2 missing.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "built wc2 missing-file exit status")
    q.command("echo alpha > grep2a.txt")
    q.command("echo beta >> grep2a.txt")
    q.command("echo gamma >> grep2a.txt")
    q.command("echo beta-two > grep2b.txt")
    out = q.command("./grep2 beta grep2a.txt", timeout=10)
    require(out, "beta", "built grep2 file match")
    forbid(out, "alpha", "built grep2 excludes nonmatch")
    out = q.command("cat grep2a.txt | ./grep2 gamma", timeout=10)
    require(out, "gamma", "built grep2 stdin match")
    out = q.command("./grep2 beta grep2a.txt grep2b.txt", timeout=10)
    require(out, "grep2a.txt:beta", "built grep2 multi first prefix")
    require(out, "grep2b.txt:beta-two", "built grep2 multi second prefix")
    q.command("./grep2 absent grep2a.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "built grep2 no-match exit status")
    q.command("./grep2 beta missing.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "built grep2 missing-file exit status")
    q.command("echo alpha > diff2a.txt")
    q.command("echo beta >> diff2a.txt")
    q.command("echo alpha > diff2b.txt")
    q.command("echo zeta >> diff2b.txt")
    out = q.command("./diff2 diff2a.txt diff2a.txt", timeout=10)
    forbid(out, "differ", "built diff2 identical files are quiet")
    out = q.command("echo $?")
    require(out, "\n0\n", "built diff2 identical exit status")
    out = q.command("./diff2 diff2a.txt diff2b.txt", timeout=10)
    require(out, "diff2a.txt diff2b.txt differ: line 2", "built diff2 line header")
    require(out, "- beta", "built diff2 removed line")
    require(out, "+ zeta", "built diff2 added line")
    out = q.command("echo $?")
    require(out, "\n1\n", "built diff2 differing exit status")
    out = q.command("./hello", timeout=10)
    require(out, "HELLO_USERLAND", "built userland hello runs")
    q.command("echo one > lines.txt")
    q.command("echo two >> lines.txt")
    q.command("echo three >> lines.txt")
    out = q.command("./lines lines.txt", timeout=10)
    require(out, "\n3\n", "built userland lines counts file")
    out = q.command("cat lines.txt | ./lines", timeout=10)
    require(out, "\n3\n", "built userland lines counts stdin")
    out = q.command("build install", timeout=10)
    require(out, "BUILD_PASS cat2-install", "build install cat2 pass")
    require(out, "BUILD_PASS diff2-install", "build install diff2 pass")
    require(out, "BUILD_PASS grep2-install", "build install grep2 pass")
    require(out, "BUILD_PASS hello-install", "build install hello pass")
    require(out, "BUILD_PASS lines-install", "build install lines pass")
    require(out, "BUILD_PASS wc2-install", "build install wc2 pass")
    out = q.command("/bin/cat2 cat2.txt", timeout=10)
    require(out, "alpha", "installed cat2 reads file first line")
    require(out, "beta", "installed cat2 reads file second line")
    out = q.command("cat cat2.txt | /bin/cat2", timeout=10)
    require(out, "alpha", "installed cat2 reads stdin first line")
    require(out, "beta", "installed cat2 reads stdin second line")
    out = q.command("/bin/wc2 wc2a.txt", timeout=10)
    require(out, "2 3 17 wc2a.txt", "installed wc2 single file counts")
    out = q.command("cat wc2a.txt | /bin/wc2", timeout=10)
    require(out, "2 3 17", "installed wc2 stdin counts")
    out = q.command("/bin/grep2 beta grep2a.txt", timeout=10)
    require(out, "beta", "installed grep2 file match")
    forbid(out, "alpha", "installed grep2 excludes nonmatch")
    out = q.command("cat grep2a.txt | /bin/grep2 gamma", timeout=10)
    require(out, "gamma", "installed grep2 stdin match")
    out = q.command("/bin/diff2 diff2a.txt diff2b.txt", timeout=10)
    require(out, "diff2a.txt diff2b.txt differ: line 2", "installed diff2 line header")
    require(out, "- beta", "installed diff2 removed line")
    require(out, "+ zeta", "installed diff2 added line")
    out = q.command("/bin/hello", timeout=10)
    require(out, "HELLO_USERLAND", "installed userland hello runs")
    out = q.command("/bin/lines lines.txt", timeout=10)
    require(out, "\n3\n", "installed userland lines counts file")
    out = q.command("cat lines.txt | /bin/lines", timeout=10)
    require(out, "\n3\n", "installed userland lines counts stdin")
    out = q.command("install lines /bin/lines2", timeout=10)
    forbid(out, "install:", "install self-built lines has no error")
    out = q.command("/bin/lines2 lines.txt", timeout=10)
    require(out, "\n3\n", "install tool installs self-built lines")
    cleanup(q, "cat2", "diff2", "grep2", "hello", "lines", "wc2", "/bin/cat2", "/bin/diff2", "/bin/grep2", "/bin/hello", "/bin/lines", "/bin/wc2")
    out = q.command("build install", timeout=90)
    require(out, "BUILD_PASS cat2", "build install auto-builds cat2")
    require(out, "BUILD_PASS diff2", "build install auto-builds diff2")
    require(out, "BUILD_PASS grep2", "build install auto-builds grep2")
    require(out, "BUILD_PASS hello", "build install auto-builds hello")
    require(out, "BUILD_PASS lines", "build install auto-builds lines")
    require(out, "BUILD_PASS wc2", "build install auto-builds wc2")
    require(out, "BUILD_PASS cat2-install", "build install installs auto-built cat2")
    require(out, "BUILD_PASS diff2-install", "build install installs auto-built diff2")
    require(out, "BUILD_PASS grep2-install", "build install installs auto-built grep2")
    require(out, "BUILD_PASS hello-install", "build install installs auto-built hello")
    require(out, "BUILD_PASS lines-install", "build install installs auto-built lines")
    require(out, "BUILD_PASS wc2-install", "build install installs auto-built wc2")
    out = q.command("/bin/wc2 wc2a.txt", timeout=10)
    require(out, "2 3 17 wc2a.txt", "auto-built installed wc2 runs")
    out = q.command("/bin/grep2 beta grep2a.txt", timeout=10)
    require(out, "beta", "auto-built installed grep2 runs")
    out = q.command("/bin/diff2 diff2a.txt diff2b.txt", timeout=10)
    require(out, "differ: line 2", "auto-built installed diff2 runs")
    out = q.command("build install", timeout=10)
    require(out, "BUILD_SKIP cat2-install", "build install cat2 skip")
    require(out, "BUILD_SKIP diff2-install", "build install diff2 skip")
    require(out, "BUILD_SKIP grep2-install", "build install grep2 skip")
    require(out, "BUILD_SKIP hello-install", "build install hello skip")
    require(out, "BUILD_SKIP lines-install", "build install lines skip")
    require(out, "BUILD_SKIP wc2-install", "build install wc2 skip")

    cleanup(
        q,
        "rt_libc",
        "rt_fs",
        "cat2",
        "diff2",
        "grep2",
        "hello",
        "lines",
        "wc2",
        "/bin/cat2",
        "/bin/diff2",
        "/bin/grep2",
        "/bin/hello",
        "/bin/lines",
        "/bin/wc2",
    )
    out = q.command("build world", timeout=120)
    require(out, "BUILD_PASS libc-contract", "build world libc contract pass")
    require(out, "BUILD_PASS fs-contract", "build world fs contract pass")
    require(out, "BUILD_PASS cat2", "build world cat2 pass")
    require(out, "BUILD_PASS diff2", "build world diff2 pass")
    require(out, "BUILD_PASS grep2", "build world grep2 pass")
    require(out, "BUILD_PASS hello", "build world hello pass")
    require(out, "BUILD_PASS lines", "build world lines pass")
    require(out, "BUILD_PASS wc2", "build world wc2 pass")
    require(out, "BUILD_PASS cat2-install", "build world cat2 install pass")
    require(out, "BUILD_PASS diff2-install", "build world diff2 install pass")
    require(out, "BUILD_PASS grep2-install", "build world grep2 install pass")
    require(out, "BUILD_PASS hello-install", "build world hello install pass")
    require(out, "BUILD_PASS lines-install", "build world lines install pass")
    require(out, "BUILD_PASS wc2-install", "build world wc2 install pass")
    require(out, "BUILD_PASS world", "build world target pass")
    out = q.command("./rt_libc", timeout=10)
    require(out, "LIBC_CONTRACT_PASS", "build world libc contract runs")
    out = q.command("./rt_fs", timeout=10)
    require(out, "FS_CONTRACT_PASS", "build world fs contract runs")
    out = q.command("/bin/wc2 wc2a.txt", timeout=10)
    require(out, "2 3 17 wc2a.txt", "build world installed wc2 runs")
    out = q.command("/bin/grep2 beta grep2a.txt", timeout=10)
    require(out, "beta", "build world installed grep2 runs")
    out = q.command("/bin/diff2 diff2a.txt diff2b.txt", timeout=10)
    require(out, "differ: line 2", "build world installed diff2 runs")

    q.command("echo bad cc /src/tests/nope.c rt_bad scratch /src/tests/nope.c > /src/Buildfile")
    out = q.command("build bad", timeout=5)
    require(out, "missing dependency for bad: /src/tests/nope.c", "build missing dependency message")
    require(out, "BUILD_FAIL bad", "build missing dependency fails")
    q.command("echo libc-contract cc /src/tests/libc_ct.c rt_libc contracts /src/tests/libc_ct.c > /src/Buildfile")
    q.command("echo fs-contract cc /src/tests/fs_ct.c rt_fs contracts /src/tests/fs_ct.c >> /src/Buildfile")
    q.command("echo cat2 cc /src/userland/cat.c cat2 userland /src/userland/cat.c >> /src/Buildfile")
    q.command("echo diff2 cc /src/userland/diff.c diff2 userland /src/userland/diff.c >> /src/Buildfile")
    q.command("echo grep2 cc /src/userland/grep.c grep2 userland /src/userland/grep.c >> /src/Buildfile")
    q.command("echo hello cc /src/userland/hello.c hello userland /src/userland/hello.c >> /src/Buildfile")
    q.command("echo lines cc /src/userland/lines.c lines userland /src/userland/lines.c >> /src/Buildfile")
    q.command("echo wc2 cc /src/userland/wc.c wc2 userland /src/userland/wc.c >> /src/Buildfile")
    q.command("echo cat2-install copy cat2 /bin/cat2 install @cat2 >> /src/Buildfile")
    q.command("echo diff2-install copy diff2 /bin/diff2 install @diff2 >> /src/Buildfile")
    q.command("echo grep2-install copy grep2 /bin/grep2 install @grep2 >> /src/Buildfile")
    q.command("echo hello-install copy hello /bin/hello install @hello >> /src/Buildfile")
    q.command("echo lines-install copy lines /bin/lines install @lines >> /src/Buildfile")
    q.command("echo wc2-install copy wc2 /bin/wc2 install @wc2 >> /src/Buildfile")
    q.command("echo world phony - - world @libc-contract,@fs-contract,@cat2-install,@diff2-install,@grep2-install,@hello-install,@lines-install,@wc2-install >> /src/Buildfile")

    q.command("build missing")
    out = q.command("echo $?")
    require(out, "\n1\n", "build unknown target exit status")

    cleanup(
        q,
        "rt_libc",
        "rt_fs",
        "build.log",
        "ct_trunc",
        "ct_stdio",
        "ct_seek",
        "fs_basic",
        "fs_trunc",
        "fs_unlink",
        "fs_seek",
        "fs_offset",
        "cat2",
        "cat2.txt",
        "diff2",
        "diff2a.txt",
        "diff2b.txt",
        "grep2",
        "grep2a.txt",
        "grep2b.txt",
        "hello",
        "lines",
        "lines.txt",
        "wc2",
        "wc2a.txt",
        "wc2b.txt",
        "lines2",
    )


def test_runtests_tool(q):
    cleanup(q, "rt_a", "rt_b", "rt_c", "rt_lines", "rt_mvsrc", "rt_mvdst", "rt_out")

    out = q.command("runtests list", timeout=5)
    require(out, "tools:", "runtests list tools group")
    require(out, "  wc", "runtests list wc")
    require(out, "  diff", "runtests list diff")
    require(out, "contracts:", "runtests list contracts group")
    require(out, "  fs-contract", "runtests list fs contract")
    require(out, "world:", "runtests list world group")
    require(out, "  build-world", "runtests list build world")
    require(out, "  diff2", "runtests list diff2")
    require(out, "  wc2", "runtests list wc2")

    out = q.command("runtests help", timeout=5)
    require(out, "usage: runtests", "runtests help usage")
    require(out, "runtests list", "runtests help mentions list")

    out = q.command("runtests", timeout=60)
    require(out, "PASS cp-cmp", "runtests cp/cmp pass")
    require(out, "PASS cmp-diff", "runtests cmp-diff pass")
    require(out, "PASS diff", "runtests diff pass")
    require(out, "PASS wc", "runtests wc pass")
    require(out, "PASS head", "runtests head pass")
    require(out, "PASS tail", "runtests tail pass")
    require(out, "PASS hexdump", "runtests hexdump pass")
    require(out, "PASS mv", "runtests mv pass")
    require(out, "PASS libc-contract", "runtests libc contract pass")
    require(out, "PASS fs-contract", "runtests fs contract pass")
    require(out, "SUMMARY 10 passed 0 failed", "runtests summary")
    require(out, "RUNTESTS_PASS", "runtests pass marker")

    out = q.command("echo $?")
    require(out, "\n0\n", "runtests exit status")

    out = q.command("runtests tools", timeout=20)
    require(out, "PASS cp-cmp", "runtests tools cp/cmp pass")
    require(out, "PASS diff", "runtests tools diff pass")
    require(out, "PASS mv", "runtests tools mv pass")
    require(out, "SUMMARY 8 passed 0 failed", "runtests tools summary")
    forbid(out, "libc-contract", "runtests tools skips contracts")

    out = q.command("runtests world", timeout=120)
    require(out, "PASS build-world", "runtests world build pass")
    require(out, "PASS world-contracts", "runtests world contracts pass")
    require(out, "PASS cat2", "runtests world cat2 pass")
    require(out, "PASS diff2", "runtests world diff2 pass")
    require(out, "PASS wc2", "runtests world wc2 pass")
    require(out, "PASS grep2", "runtests world grep2 pass")
    require(out, "PASS lines", "runtests world lines pass")
    require(out, "PASS hello", "runtests world hello pass")
    require(out, "SUMMARY 8 passed 0 failed", "runtests world summary")
    require(out, "RUNTESTS_PASS", "runtests world pass marker")

    out = q.command("runtests wc", timeout=10)
    require(out, "PASS wc", "runtests single wc pass")
    require(out, "SUMMARY 1 passed 0 failed", "runtests single summary")
    forbid(out, "PASS head", "runtests single skips other tests")

    q.command("runtests missing")
    out = q.command("echo $?")
    require(out, "\n1\n", "runtests unknown selector exit status")


def test_vi_save_and_keys(q):
    cleanup(q, "save.txt", "esc.txt", "keys.txt", "bs.txt")
    q.vi("save.txt", b"ilong-long-long-line\nanother-long-line\n\x1b:wq\r", timeout=8)
    out = q.command("cat save.txt")
    require(out, "long-long-long-line", "initial vi save")

    q.vi("save.txt", b":%d\riS\x1b:wq\r", timeout=8)
    out = q.command("cat save.txt")
    require(out, "\nS\n", "short vi save")
    forbid(out, "long-long", "stale vi file tail")

    q.open_vi("esc.txt")
    q.send(b"iabc")
    q.read_for(0.3)
    q.send(b"\x1b")
    out = q.read_for(1.0)
    require(out, "N", "visible normal mode after bare ESC")
    q.send(b":wq\r")
    q.read_until(PROMPT, timeout=5, label="prompt after esc.txt")

    q.vi("keys.txt", b"iabc\x1b[DX\x1b:wq\r", timeout=8)
    out = q.command("cat keys.txt")
    require(out, "abXc", "arrow-key insertion position")

    q.vi("bs.txt", b"iA\t\nB\x1b:wq\r", timeout=8)
    q.vi("bs.txt", b"i\x1b[C\x1b[C\x7f\x1b[B\x1b[D\x7f\x1b:wq\r", timeout=8)
    out = q.command("cat bs.txt")
    require(out, "\nAB\n", "backspace deletes tab and joins lines")


def test_tcc_scanf(q):
    cleanup(q, "hello.c", "hello")
    source = (
        b"i#include <stdio.h>\n"
        b"int main(){\n"
        b" int x;\n"
        b" scanf(\"%d\", &x);\n"
        b" printf(\"x=%d\\n\", x);\n"
        b" return 0;\n"
        b"}\n"
        b"\x1b:wq\r"
    )
    q.vi("hello.c", source, timeout=10)
    out = q.command("tcc hello.c -o hello", timeout=30)
    forbid(out, "error:", "tcc error")
    forbid(out, "unresolved reference", "unresolved scanf")

    q.send("./hello\r")
    q.read_for(0.2)
    q.send("42\r")
    out = q.read_until(PROMPT, timeout=8, label="prompt after ./hello")
    require(out, "42", "scanf input echo")
    require(out, "x=42", "scanf runtime result")


def test_cxx_contract(q):
    out = q.command("cat /src/tests/cxx_ct.cc", timeout=5)
    require(out, "CXX_CONTRACT_PASS", "cxx contract source in image")
    require(out, "class Multiplier", "cxx contract class source")

    out = q.command("cxxtest", timeout=10)
    require(out, "CXX_CONTRACT_PASS", "cxx contract pass marker")
    forbid(out, "CXX_CONTRACT_FAIL", "cxx contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "cxx contract exit status")


def test_gcc_contract(q):
    out = q.command("gcctest", timeout=10)
    require(out, "LIBC_CONTRACT_PASS", "gcc-built libc contract pass marker")
    forbid(out, "LIBC_CONTRACT_FAIL", "gcc-built libc contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "gcc-built libc contract exit status")


def test_gcc_static_contract(q):
    cleanup(q, "gs_stdio", "gs_raw", "gs_ren", "gs_exec", "gs_dup2")
    q.command("rm gs_dir", timeout=5)

    out = q.command("cat /src/tests/gccst_main.c", timeout=5)
    require(out, "GCC_STATIC_PASS", "gcc static main source in image")
    require(out, "test_pipe_fork_wait", "gcc static main covers pipe/fork/wait")
    require(out, "test_dup2_redirect", "gcc static main covers dup2 redirect")
    require(out, "getcwd-dir", "gcc static main covers getcwd/chdir")
    require(out, "test_posix_host_helpers", "gcc static main covers POSIX host helpers")
    out = q.command("cat /src/tests/gccst_lib.c", timeout=5)
    require(out, "gcc_static_count_words", "gcc static lib source in image")
    out = q.command("cat /src/tests/gccst_lib.h", timeout=5)
    require(out, "gcc_static_weighted_arg_len", "gcc static header in image")

    out = q.command("gccstatictest alpha beta", timeout=15)
    require(out, "GCC_STATIC_PASS", "gcc static contract pass marker")
    forbid(out, "GCC_STATIC_FAIL", "gcc static contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "gcc static contract exit status")


def test_gcc_userland(q):
    cleanup(q, "gcc_a.txt", "gcc_b.txt", "gcc_lines.txt")

    out = q.command("gcc-hello", timeout=10)
    require(out, "HELLO_USERLAND", "gcc-built hello output")

    q.command("echo alpha beta > gcc_a.txt")
    q.command("echo gamma >> gcc_a.txt")
    q.command("echo alpha beta > gcc_b.txt")
    q.command("echo zeta >> gcc_b.txt")

    out = q.command("gcc-cat gcc_a.txt", timeout=10)
    require(out, "alpha beta", "gcc-built cat reads first line")
    require(out, "gamma", "gcc-built cat reads second line")

    out = q.command("cat gcc_a.txt | gcc-cat", timeout=10)
    require(out, "alpha beta", "gcc-built cat reads stdin")
    require(out, "gamma", "gcc-built cat stdin second line")

    out = q.command("gcc-wc gcc_a.txt", timeout=10)
    require(out, "2 3 17 gcc_a.txt", "gcc-built wc counts file")

    out = q.command("cat gcc_a.txt | gcc-wc", timeout=10)
    require(out, "2 3 17", "gcc-built wc counts stdin")

    out = q.command("gcc-grep beta gcc_a.txt", timeout=10)
    require(out, "alpha beta", "gcc-built grep file match")
    forbid(out, "gamma", "gcc-built grep excludes nonmatch")

    out = q.command("cat gcc_a.txt | gcc-grep gamma", timeout=10)
    require(out, "gamma", "gcc-built grep stdin match")

    q.command("echo one > gcc_lines.txt")
    q.command("echo two >> gcc_lines.txt")
    q.command("echo three >> gcc_lines.txt")
    out = q.command("gcc-lines gcc_lines.txt", timeout=10)
    require(out, "\n3\n", "gcc-built lines counts file")

    out = q.command("cat gcc_lines.txt | gcc-lines", timeout=10)
    require(out, "\n3\n", "gcc-built lines counts stdin")

    out = q.command("gcc-diff gcc_a.txt gcc_b.txt", timeout=10)
    require(out, "gcc_a.txt gcc_b.txt differ: line 2", "gcc-built diff line header")
    require(out, "- gamma", "gcc-built diff removed line")
    require(out, "+ zeta", "gcc-built diff added line")

    q.command("gcc-grep absent gcc_a.txt")
    out = q.command("echo $?")
    require(out, "\n1\n", "gcc-built grep no-match exit status")


def test_cxx_std_contract(q):
    out = q.command("cat /src/tests/cxxstd_ct.cc", timeout=5)
    require(out, "CXX_STD_CONTRACT_PASS", "cxx std contract source in image")
    require(out, "std::array", "cxx std contract uses array")

    out = q.command("cat /include/c++/array", timeout=5)
    require(out, "namespace std", "cxx array header in image")
    out = q.command("cat /include/c++/algorithm", timeout=5)
    require(out, "sort", "cxx algorithm header in image")

    out = q.command("cxxstdtest", timeout=10)
    require(out, "CXX_STD_CONTRACT_PASS", "cxx std contract pass marker")
    forbid(out, "CXX_STD_CONTRACT_FAIL", "cxx std contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "cxx std contract exit status")


def test_cxx_dyn_contract(q):
    out = q.command("cat /src/tests/cxxdyn_ct.cc", timeout=5)
    require(out, "CXX_DYN_CONTRACT_PASS", "cxx dyn contract source in image")
    require(out, "std::vector", "cxx dyn contract uses vector")
    require(out, "std::string", "cxx dyn contract uses string")

    out = q.command("cat /include/c++/vector", timeout=5)
    require(out, "class vector", "cxx vector header in image")
    out = q.command("cat /include/c++/string", timeout=5)
    require(out, "class string", "cxx string header in image")

    out = q.command("cxxdyntest", timeout=10)
    require(out, "CXX_DYN_CONTRACT_PASS", "cxx dyn contract pass marker")
    forbid(out, "CXX_DYN_CONTRACT_FAIL", "cxx dyn contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "cxx dyn contract exit status")


def test_gxx_contract(q):
    out = q.command("cat /src/tests/gxx_ct.cc", timeout=5)
    require(out, "GXX_CONTRACT_PASS", "gxx contract source in image")
    require(out, "snprintf", "gxx contract uses libc header")

    out = q.command("gxxtest", timeout=10)
    require(out, "GXX_CONTRACT_PASS cpp-gxx-myos:3", "gxx contract pass marker")
    forbid(out, "GXX_CONTRACT_FAIL", "gxx contract failure marker")
    out = q.command("echo $?")
    require(out, "\n0\n", "gxx contract exit status")


def test_libc_contract(q):
    cleanup(q, "libc_contract", "ct_trunc", "ct_stdio", "ct_seek")
    out = q.command("tcc /src/tests/libc_ct.c -o libc_contract", timeout=30)
    forbid(out, "error:", "libc contract compile error")
    forbid(out, "unresolved reference", "libc contract unresolved reference")

    out = q.command("./libc_contract", timeout=10)
    require(out, "LIBC_CONTRACT_PASS", "libc contract pass marker")
    forbid(out, "LIBC_CONTRACT_FAIL", "libc contract failure marker")


def test_fs_contract(q):
    cleanup(
        q,
        "fs_contract",
        "fs_basic",
        "fs_trunc",
        "fs_unlink",
        "fs_seek",
        "fs_offset",
        "fsra",
        "fsrb",
        "fsrc",
        "fsrd",
        "fsre",
        "fsrf",
        "fsrg",
        "fsrh",
        "fsri",
        "fsrj",
        "fsrk",
        "fsrl",
        "fsrm",
        "fsrn",
        "fsro",
        "fsrp",
    )
    out = q.command("tcc /src/tests/fs_ct.c -o fs_contract", timeout=30)
    forbid(out, "error:", "fs contract compile error")
    forbid(out, "unresolved reference", "fs contract unresolved reference")

    out = q.command("./fs_contract", timeout=10)
    require(out, "FS_CONTRACT_PASS", "fs contract pass marker")
    forbid(out, "FS_CONTRACT_FAIL", "fs contract failure marker")


def test_tty_contract(q):
    cleanup(q, "tty_contract", "ttyvi.txt")
    out = q.command("tcc /src/tests/tty_ct.c -o tty_contract", timeout=30)
    forbid(out, "error:", "tty contract compile error")
    forbid(out, "unresolved reference", "tty contract unresolved reference")

    q.send("./tty_contract\r")
    first = q.read_until(b"TTY_NOECHO_READY", timeout=5, label="tty noecho ready")
    q.send("secret\r")
    second = q.read_until(b"TTY_ECHO_READY", timeout=5, label="tty echo ready")
    noecho_flow = first + second
    require(noecho_flow, "TTY_NOECHO_GOT:secret", "tty noecho input")
    forbid_between(
        noecho_flow,
        "TTY_NOECHO_READY",
        "TTY_NOECHO_GOT:secret",
        "secret",
        "input echo while ECHO is disabled",
    )

    q.send("visible\r")
    third = q.read_until(PROMPT, timeout=5, label="prompt after tty contract")
    echo_flow = second + third
    require(third, "TTY_CONTRACT_PASS", "tty contract pass marker")
    forbid(third, "TTY_CONTRACT_FAIL", "tty contract failure marker")
    require_between(
        echo_flow,
        "TTY_ECHO_READY",
        "TTY_ECHO_GOT:visible",
        "visible",
        "input echo after ECHO restore",
    )

    q.send("./tty_contract block\r")
    q.read_until(b"TTY_BLOCK_READY", timeout=5, label="tty block ready")
    q.send(b"\x03")
    out = q.read_until(PROMPT, timeout=8, label="prompt after tty Ctrl-C")
    forbid(out, "TTY_BLOCK_AFTER_READ", "blocked read continuing after Ctrl-C")

    q.open_vi("ttyvi.txt")
    q.read_for(0.5)
    q.send(b":q!\r")
    out = q.read_until(PROMPT, timeout=5, label="prompt after vi exit")
    require_raw(out, b"\x1b[?1049l", "vi restores main screen on exit")
    forbid_raw(out, b"\x1b[2J", "vi exit clear-screen sequence")


def main():
    parser = argparse.ArgumentParser(description="Run MyOS QEMU smoke tests.")
    parser.add_argument("--kernel", default="kernel.elf")
    parser.add_argument("--fs", default="fs.img")
    parser.add_argument("--qemu", default="qemu-system-riscv64")
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--keep-image", action="store_true")
    args = parser.parse_args()

    workdir = tempfile.mkdtemp(prefix="myos-qemu-smoke-")
    image = os.path.join(workdir, "fs.img")
    shutil.copyfile(args.fs, image)

    q = None
    tests = [
        ("shell", test_shell),
        ("grep", test_grep_tool),
        ("wc", test_wc_tool),
        ("cp", test_cp_tool),
        ("install", test_install_tool),
        ("mv", test_mv_tool),
        ("cmp", test_cmp_tool),
        ("diff", test_diff_tool),
        ("head-tail", test_head_tail_tools),
        ("hexdump", test_hexdump_tool),
        ("build", test_build_tool),
        ("runtests", test_runtests_tool),
        ("vi", test_vi_save_and_keys),
        ("tcc-scanf", test_tcc_scanf),
        ("gcc-contract", test_gcc_contract),
        ("gcc-static-contract", test_gcc_static_contract),
        ("gcc-userland", test_gcc_userland),
        ("cxx-contract", test_cxx_contract),
        ("cxx-std-contract", test_cxx_std_contract),
        ("cxx-dyn-contract", test_cxx_dyn_contract),
        ("gxx-contract", test_gxx_contract),
        ("libc-contract", test_libc_contract),
        ("fs-contract", test_fs_contract),
        ("tty-contract", test_tty_contract),
    ]
    try:
        q = Qemu(args.qemu, args.kernel, image, args.timeout)
        q.read_until(PROMPT, timeout=args.timeout, label="initial shell prompt")
        for name, fn in tests:
            print(f"[TEST] {name}")
            fn(q)
            print(f"[PASS] {name}")
        print("[PASS] qemu smoke tests")
        return 0
    except TestFailure as exc:
        print(f"[FAIL] {exc}", file=sys.stderr)
        if args.keep_image:
            print(f"[INFO] kept image: {image}", file=sys.stderr)
        return 1
    finally:
        if q is not None:
            q.close()
        if not args.keep_image:
            shutil.rmtree(workdir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
