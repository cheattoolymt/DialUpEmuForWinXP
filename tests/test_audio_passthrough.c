/* SPDX-License-Identifier: BSD-2-Clause
 * Real common renderer: equal-rate output preserves sample count/waveform,
 * volume, clipping, channel mapping, silence and flush without FIR allocation. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_audio_internal.h"
static void run(int channels)
{
    vm_audio_t a;
    float input[160], output[320];
    unsigned i;
    memset(&a,0,sizeof(a));
    a.params.dsp_rate=a.device_rate=8000;
    a.params.volume=0.5f;
    a.device_channels=channels;
    assert(vm_frb_init(&a.ring,512)==VM_OK);
    assert(vm_audio_backend_resampler_init(&a)==VM_OK);
    assert(a.rs_ready && a.rs.hist==NULL); /* no filtering or table setup */
    for(i=0;i<160;++i) input[i]=((int)(i%17)-8)*0.4f;
    assert(vm_frb_write(&a.ring,input,160)==160);
    vm_audio_backend_render(&a,output,160);
    assert(vm_frb_used(&a.ring)==0);
    for(i=0;i<160;++i) {
        float expected=input[i]*0.5f;
        int c;
        if(expected>1)expected=1;
        if(expected< -1)expected= -1;
        for(c=0;c<channels;++c) assert(output[i*channels+c]==expected);
    }
    vm_audio_backend_render(&a,output,160);
    for(i=0;i<160*(unsigned)channels;++i) assert(output[i]==0);
    assert(vm_frb_write(&a.ring,input,160)==160);
    a.flush_req=1;
    vm_audio_backend_handle_flush(&a);
    assert(vm_frb_used(&a.ring)==0 && !a.flush_req);
    vm_resampler_free(&a.rs);
    vm_frb_free(&a.ring);
}
int main(void)
{
    run(1);run(2);
    puts("PASS: 8 kHz mono/stereo preserve samples, volume/clip, silence and flush without FIR");
    return 0;
}
