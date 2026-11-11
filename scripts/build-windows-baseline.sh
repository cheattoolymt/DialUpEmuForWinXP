#!/usr/bin/env bash
# Reproduce the copied modern Windows x64 build, NOT an XP build.
set -euo pipefail
cd "$(dirname "$0")/.."
cc=${CC:-x86_64-w64-mingw32-gcc}
objdump=${OBJDUMP:-x86_64-w64-mingw32-objdump}
mkdir -p work/windows-baseline/obj
# --no-ignore: the inherited .gitignore pattern "core" also matches src/core/.
mapfile -t sources < <(rg --files --no-ignore src | sort | sed -n '/\.c$/p')
objects=()
for source in "${sources[@]}"; do
    object="work/windows-baseline/obj/${source#src/}"
    object="${object%.c}.o"
    mkdir -p "$(dirname "$object")"
    "$cc" -O2 -std=c99 -Wall -Wextra -Werror -Iinclude \
        -DVMODEM_HAVE_LIBSLIRP -c "$source" -o "$object"
    objects+=("$object")
done
library_path=()
if [[ -n ${SLIRP_LIBDIR:-} ]]; then library_path+=("-L$SLIRP_LIBDIR"); fi
"$cc" "${objects[@]}" -o work/windows-baseline/vmodem.exe "${library_path[@]}" \
    -lslirp -lws2_32 -liphlpapi -lole32 -lwinmm -lfwpuclnt -lm
"$objdump" -p work/windows-baseline/vmodem.exe > work/windows-baseline/imports.txt
printf '%s\n' 'PASS: copied Windows x64 baseline compiled/linked; XP runtime is NOT validated.'
