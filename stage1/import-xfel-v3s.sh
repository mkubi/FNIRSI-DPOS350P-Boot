#!/bin/sh
set -eu

XFEL_ROOT=${1:-$HOME/xfel}
SRC="$XFEL_ROOT/payloads/v3s_s3/v3s-ddr"
DST="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/xfel-v3s"

for file in \
    "$SRC/source/sys-clock.c" \
    "$SRC/source/sys-dram.c" \
    "$SRC/source/memcpy.S" \
    "$SRC/source/memset.S" \
    "$SRC/include/xboot.h" \
    "$SRC/include/types.h" \
    "$SRC/include/io.h" \
    "$SRC/include/string.h" \
    "$SRC/include/stdint.h" \
    "$SRC/include/stddef.h" \
    "$SRC/include/stdarg.h" \
    "$SRC/include/endian.h" \
    "$SRC/include/byteorder.h" \
    "$SRC/include/v3s/reg-ccu.h" \
    "$SRC/include/v3s/reg-dram.h"
do
    if [ ! -f "$file" ]; then
        echo "Missing XFEL source: $file" >&2
        exit 1
    fi
done

rm -rf "$DST"
mkdir -p "$DST/source" "$DST/include/v3s"

cp "$SRC/source/sys-clock.c" "$DST/source/"
cp "$SRC/source/sys-dram.c" "$DST/source/"
cp "$SRC/source/memcpy.S" "$DST/source/"
cp "$SRC/source/memset.S" "$DST/source/"
cp "$SRC/include/"*.h "$DST/include/"
cp "$SRC/include/v3s/"*.h "$DST/include/v3s/"

printf '%s\n' \
    "Imported exact V3s/S3 DDR sources from:" \
    "$SRC" \
    "into:" \
    "$DST"
