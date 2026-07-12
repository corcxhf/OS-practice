#!/bin/sh
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SYSROOT=${MYOS_CXX_SYSROOT:-"$SELF_DIR/cxx-sysroot"}
BACKEND_CXX=${MYOS_GXX_BACKEND:-riscv64-unknown-elf-g++}

COMMON_FLAGS="-Os -nostdlib -nostartfiles -ffreestanding -std=c++20 -fno-exceptions -fno-rtti -fno-use-cxa-atexit -fno-stack-protector -fno-threadsafe-statics -mno-relax -msmall-data-limit=0 -fno-pic -fno-pie -mcmodel=medany -march=rv64gc -mabi=lp64d"
LIBGCC=$("$BACKEND_CXX" $COMMON_FLAGS -print-libgcc-file-name)

LINK=1
for arg in "$@"; do
    case "$arg" in
    -c|-S|-E|-shared|-r)
        LINK=0
        ;;
    esac
done

if [ "$LINK" -eq 1 ]; then
    exec "$BACKEND_CXX" $COMMON_FLAGS -nostdinc -nostdinc++ -isystem "$SYSROOT/include" -isystem "$SYSROOT/include/c++" -Wl,-e,_start -Wl,--gc-sections -Wl,-s "$SYSROOT/lib/cxx-crt0.o" "$@" "$SYSROOT/lib/libcxxrt.a" "$SYSROOT/lib/libc.a" "$LIBGCC"
fi

exec "$BACKEND_CXX" $COMMON_FLAGS -nostdinc -nostdinc++ -isystem "$SYSROOT/include" -isystem "$SYSROOT/include/c++" "$@"
