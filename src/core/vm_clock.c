/* SPDX-License-Identifier: BSD-2-Clause */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "vmodem/vm_clock.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

uint64_t vm_tick_extend(vm_tick_extender_t *clock, uint32_t tick)
{
    if (!clock->initialized) {
        clock->elapsed = tick;
        clock->initialized = true;
    } else {
        clock->elapsed += (uint32_t)(tick - clock->last);
    }
    clock->last = tick;
    return clock->elapsed;
}

uint64_t vm_clock_now_ms64(void)
{
#ifdef _WIN32
    /* GetTickCount exists on XP. Serialize both sampling and extension so
     * concurrent callers cannot interpret an older sample as a rollover. */
    static volatile LONG state;
    static CRITICAL_SECTION lock;
    static vm_tick_extender_t clock;
    uint64_t now;
    if (InterlockedCompareExchange(&state, 1, 0) == 0) {
        InitializeCriticalSection(&lock);
        InterlockedExchange(&state, 2);
    } else {
        while (InterlockedCompareExchange(&state, 2, 2) != 2) Sleep(0);
    }
    EnterCriticalSection(&lock);
    now = vm_tick_extend(&clock, GetTickCount());
    LeaveCriticalSection(&lock);
    return now;
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}
