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


def test_runtests_tool(q):
    cleanup(q, "rt_a", "rt_b", "rt_c", "rt_lines", "rt_mvsrc", "rt_mvdst", "rt_out")

    out = q.command("runtests list", timeout=5)
    require(out, "tools:", "runtests list tools group")
    require(out, "  wc", "runtests list wc")
    require(out, "contracts:", "runtests list contracts group")
    require(out, "  fs-contract", "runtests list fs contract")

    out = q.command("runtests help", timeout=5)
    require(out, "usage: runtests", "runtests help usage")
    require(out, "runtests list", "runtests help mentions list")

    out = q.command("runtests", timeout=60)
    require(out, "PASS cp-cmp", "runtests cp/cmp pass")
    require(out, "PASS cmp-diff", "runtests cmp-diff pass")
    require(out, "PASS wc", "runtests wc pass")
    require(out, "PASS head", "runtests head pass")
    require(out, "PASS tail", "runtests tail pass")
    require(out, "PASS hexdump", "runtests hexdump pass")
    require(out, "PASS mv", "runtests mv pass")
    require(out, "PASS libc-contract", "runtests libc contract pass")
    require(out, "PASS fs-contract", "runtests fs contract pass")
    require(out, "SUMMARY 9 passed 0 failed", "runtests summary")
    require(out, "RUNTESTS_PASS", "runtests pass marker")

    out = q.command("echo $?")
    require(out, "\n0\n", "runtests exit status")

    out = q.command("runtests tools", timeout=20)
    require(out, "PASS cp-cmp", "runtests tools cp/cmp pass")
    require(out, "PASS mv", "runtests tools mv pass")
    require(out, "SUMMARY 7 passed 0 failed", "runtests tools summary")
    forbid(out, "libc-contract", "runtests tools skips contracts")

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
        ("mv", test_mv_tool),
        ("cmp", test_cmp_tool),
        ("head-tail", test_head_tail_tools),
        ("hexdump", test_hexdump_tool),
        ("runtests", test_runtests_tool),
        ("vi", test_vi_save_and_keys),
        ("tcc-scanf", test_tcc_scanf),
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
