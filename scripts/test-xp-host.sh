#!/usr/bin/env bash
# Host tests use a deterministic WinMM model, not a real Windows driver.
set -euo pipefail
cd "$(dirname "$0")/.."
cc=${CC:-gcc}
mkdir -p work/xp-host-tests
flags=(-std=c99 -Wall -Wextra -Werror)
"$cc" "${flags[@]}" -Iinclude tests/test_clock.c src/core/vm_clock.c \
    -o work/xp-host-tests/test_clock
work/xp-host-tests/test_clock
"$cc" "${flags[@]}" -D_WIN32 -Itests/fake-winmm -Iinclude -Isrc/audio \
    tests/test_waveout.c src/audio/vm_audio_waveout.c -o work/xp-host-tests/test_waveout
work/xp-host-tests/test_waveout
"$cc" "${flags[@]}" -Iinclude -Isrc/audio tests/test_audio_passthrough.c \
    src/audio/vm_audio.c src/audio/vm_audio_null.c src/core/vm_ringbuf.c \
    src/core/vm_log.c src/core/vm_types.c src/dsp/vm_resample.c -pthread -lm \
    -o work/xp-host-tests/test_audio_passthrough
work/xp-host-tests/test_audio_passthrough
