#!/usr/bin/env bash
# XP SP3 x86 offline diagnostic (Steps 3-5), not an Internet-capable release.
set -euo pipefail
cd "$(dirname "$0")/.."
cc=${CC:-i686-w64-mingw32-gcc-win32}
objdump=${OBJDUMP:-i686-w64-mingw32-objdump}
build=work/xp-build
mkdir -p "$build/obj"
flags=(-O2 -std=c99 -Wall -Wextra -Werror -march=i686 -Iinclude -Isrc/serial
       -DVMODEM_TARGET_XP -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DNTDDI_VERSION=0x05010300)
link=(-static -static-libgcc -mconsole -Wl,--major-os-version,5,--minor-os-version,1
      -Wl,--major-subsystem-version,5,--minor-subsystem-version,1)
mapfile -t sources < <(rg --files --no-ignore src | sort | sed -n '/\.c$/p' |
    sed '\|^src/audio/vm_audio_wasapi.c$|d;\|^src/net/vm_hostroute.c$|d')
objects=(); library=()
for source in "${sources[@]}"; do
    object="$build/obj/${source#src/}"; object="${object%.c}.o"
    mkdir -p "$(dirname "$object")"
    "$cc" "${flags[@]}" -c "$source" -o "$object"
    objects+=("$object")
    if [[ $source != src/main.c ]]; then library+=("$object"); fi
done
"$cc" "${objects[@]}" "${link[@]}" -o "$build/vmodem-xp.exe" -lwinmm -lws2_32 -lm
"$cc" "${flags[@]}" tests/test_xp_smoke.c "${library[@]}" "${link[@]}" \
    -o "$build/xp-smoke.exe" -lwinmm -lws2_32 -lm
for exe in vmodem-xp xp-smoke; do
    "$objdump" -p "$build/$exe.exe" > "$build/$exe-imports.txt"
    python3 scripts/audit-xp-pe.py "$build/$exe.exe" --objdump "$objdump" --report "$build/$exe-pe.json"
done
cp config-xp.ini "$build/"
printf '%s\n' 'PASS: x86 diagnostic built/audited. XP runtime, COM drivers, and physical audio are NOT yet validated.'
