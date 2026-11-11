/* SPDX-License-Identifier: BSD-2-Clause */
#include <assert.h>
#include <stdio.h>
#include "vmodem/vm_clock.h"
int main(void)
{
    vm_tick_extender_t clock = {0};
    uint64_t before, after;
    assert(vm_tick_extend(&clock, 0xfffffff0u) == 0xfffffff0ULL);
    assert(vm_tick_extend(&clock, 0xfffffff0u) == 0xfffffff0ULL);
    assert(vm_tick_extend(&clock, 0x10u) == 0x100000010ULL);
    assert(vm_tick_extend(&clock, 0xfffffffeu) == 0x1fffffffeULL);
    assert(vm_tick_extend(&clock, 2u) == 0x200000002ULL);
    before = vm_clock_now_ms64();
    after = vm_clock_now_ms64();
    assert(after >= before);
    puts("PASS: repeated ticks, successive 32-bit rollovers and monotonic host clock");
    return 0;
}
