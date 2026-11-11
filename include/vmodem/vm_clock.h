/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef VMODEM_VM_CLOCK_H
#define VMODEM_VM_CLOCK_H
#include "vmodem/vm_types.h"
/* Sample at least once per 49.7 days. The pure extender is also used by tests. */
typedef struct {
    uint32_t last;
    uint64_t elapsed;
    bool initialized;
} vm_tick_extender_t;
uint64_t vm_tick_extend(vm_tick_extender_t *clock, uint32_t tick);
uint64_t vm_clock_now_ms64(void);
#endif
