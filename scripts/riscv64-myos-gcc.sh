#!/bin/sh
set -eu

SELF_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SYSROOT=${MYOS_SYSROOT:-"$SELF_DIR/sysroot"}
CC=${CC:-riscv64-unknown-elf-gcc}

COMMON_FLAGS="-Os -nostdlib -nostartfiles -ffreestanding -fno-builtin -fno-stack-protector -mno-relax -msmall-data-limit=0 -fno-pic -fno-pie -mcmodel=medany -march=rv64gc -mabi=lp64d"
INCLUDE_FLAGS="-nostdinc -isystem $SYSROOT/include"

LINK=1
for arg in "$@"; do
    case "$arg" in
    -c|-S|-E|-shared|-r)
        LINK=0
        ;;
    esac
done

if [ "$LINK" -eq 1 ]; then
    exec "$CC" $COMMON_FLAGS $INCLUDE_FLAGS -Wl,-e,_start -Wl,--gc-sections -Wl,-s "$SYSROOT/lib/crt1.o" "$SYSROOT/lib/crti.o" "$@" "$SYSROOT/lib/libc.a" "$SYSROOT/lib/crtn.o"
fi

exec "$CC" $COMMON_FLAGS $INCLUDE_FLAGS "$@"
