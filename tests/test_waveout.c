/* SPDX-License-Identifier: BSD-2-Clause
 * Run the production waveOut worker against a deterministic device model.
 * Validate buffer ownership, PCM conversion, drain, flush and error cleanup. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "vm_audio_internal.h"
#include <mmsystem.h>
static vm_audio_t *audio;
static WAVEHDR *headers[3];
static int mode, prepares, unprepares, writes, resets, closes, waits, events, input;
static int ready_notifications, event_token;
HANDLE CreateEventW(void *s, BOOL manual, BOOL initial, const wchar_t *name)
{ (void)s;(void)manual;(void)initial;(void)name; ++events; return &event_token; }
BOOL CloseHandle(HANDLE h) { assert(h == &event_token); --events; return 1; }
DWORD WaitForSingleObject(HANDLE h, DWORD timeout)
{
    int i;
    assert(h == &event_token); (void)timeout;
    for (i = 0; i < 3; ++i) if (headers[i]) {
        headers[i]->dwFlags &= ~WHDR_INQUEUE;
        headers[i]->dwFlags |= WHDR_DONE;
    }
    if (++waits == 1) audio->flush_req = 1;
    if (waits == 2) { audio->drain_req = 1; input = 0; }
    if (waits >= 3) audio->quit = 1;
    return 0;
}
int WideCharToMultiByte(UINT cp, DWORD f, const wchar_t *src, int len,
                       char *out, int n, const char *d, BOOL *b)
{
    int i=0; (void)cp;(void)f;(void)len;(void)d;(void)b;
    while (src[i] && i<n-1) { out[i]=(char)src[i]; ++i; } out[i]=0;
    return i+1;
}
UINT waveOutGetNumDevs(void) { return 1; }
MMRESULT waveOutGetDevCapsW(UINT d, WAVEOUTCAPSW *c, UINT n)
{ (void)d;(void)n; wcscpy(c->szPname,L"Test Device"); return 0; }
MMRESULT waveOutOpen(HWAVEOUT *o, UINT dev, const WAVEFORMATEX *f,
                     DWORD_PTR cb, DWORD_PTR inst, DWORD flags)
{
    (void)dev; (void)inst;
    assert(cb == (DWORD_PTR)&event_token && flags == CALLBACK_EVENT);
    assert(f->wFormatTag == WAVE_FORMAT_PCM && f->wBitsPerSample == 16);
    assert(f->nBlockAlign == f->nChannels*2);
    assert(f->nAvgBytesPerSec == f->nSamplesPerSec*f->nBlockAlign);
    if (mode == 1 || (mode == 6 && f->nSamplesPerSec == 48000)) return WAVERR_BADFORMAT;
    *o = &event_token; return 0;
}
MMRESULT waveOutPrepareHeader(HWAVEOUT o, WAVEHDR *h, UINT size)
{
    (void)o; assert(size == sizeof(*h));
    if (mode == 2 && prepares == 1) return MMSYSERR_ERROR;
    assert(prepares < 3); headers[prepares++] = h;
    h->dwFlags = WHDR_PREPARED; return 0;
}
MMRESULT waveOutWrite(HWAVEOUT o, WAVEHDR *h, UINT size)
{
    int16_t *pcm = (int16_t *)h->lpData;
    (void)o; (void)size;
    assert(h->dwFlags & WHDR_PREPARED);
    assert(!(h->dwFlags & WHDR_INQUEUE));
    assert(pcm[0] == -32767 && pcm[1] == -16383 && pcm[2] == 16383 && pcm[3] == 32767);
    ++writes;
    if (mode == 3 && writes == 2) return MMSYSERR_ERROR;
    h->dwFlags &= ~WHDR_DONE; h->dwFlags |= WHDR_INQUEUE; return 0;
}
MMRESULT waveOutReset(HWAVEOUT o)
{
    int i; (void)o; ++resets;
    if (mode == 4 && resets == 1) return MMSYSERR_ERROR;
    for (i=0;i<prepares;++i) {
        assert(headers[i]);
        headers[i]->dwFlags &= ~WHDR_INQUEUE;
        headers[i]->dwFlags |= WHDR_DONE;
    }
    return 0;
}
MMRESULT waveOutUnprepareHeader(HWAVEOUT o, WAVEHDR *h, UINT size)
{
    (void)o;(void)size;
    assert(!(h->dwFlags & WHDR_INQUEUE));
    if (mode == 5 && unprepares++ == 0) return WAVERR_STILLPLAYING;
    if (mode != 5) ++unprepares;
    h->dwFlags &= ~WHDR_PREPARED; return 0;
}
MMRESULT waveOutClose(HWAVEOUT o)
{
    int i; (void)o;
    for(i=0;i<prepares;++i) assert(!(headers[i]->dwFlags & (WHDR_INQUEUE|WHDR_PREPARED)));
    ++closes; return 0;
}
vm_err_t vm_audio_backend_resampler_init(vm_audio_t *a) { (void)a; return VM_OK; }
uint32_t vm_frb_used(const vm_frb_t *r) { (void)r; return input ? 160 : 0; }
void vm_audio_backend_render(vm_audio_t *a, float *out, uint32_t frames)
{
    static const float sample[] = {-2.0f,-0.5f,0.5f,2.0f};
    uint32_t i; for(i=0;i<frames*(uint32_t)a->device_channels;++i) out[i]=sample[i%4];
}
void vm_audio_backend_handle_flush(vm_audio_t *a) { assert(a->device_pending == 0); a->flush_req=0; }
void vm_ev_set(vm_event_t e) { if (e == audio->ready_ev) ++ready_notifications; }
void vm_log_write(vm_log_level_t l,const char *f,int line,const char *fmt,...)
{ (void)l;(void)f;(void)line;(void)fmt; }
int main(void)
{
    int m;
    for (m=0;m<=7;++m) {
        vm_audio_t a;
        memset(&a,0,sizeof(a)); memset(headers,0,sizeof(headers));
        audio=&a; mode=m; prepares=unprepares=writes=resets=closes=waits=events=0;
        ready_notifications=0; input=1; a.ready_ev=&a; a.space_ev=NULL;
        a.params.dsp_rate=8000;
        if (m==7) a.params.device_match="missing device";
        else if (m==6) a.params.device_match="test DEVICE";
        vm_audio_run_waveout(&a);
        assert(ready_notifications==1 && events==0 && a.impl==NULL);
        assert(a.device_pending==0);
        if (m==1 || m==2 || m==7) assert(a.ready_result!=VM_OK);
        else {
            assert(a.ready_result==VM_OK);
            if (m==3 || m==4) assert(a.failed);
            else assert(!a.failed && writes==6 && unprepares>0);
        }
        if (m!=1 && m!=7) assert(closes==1);
        if (m==6) assert(a.device_rate==44100);
    }
    puts("PASS: waveOut PCM, drain/flush, format/name selection and 4 failure/cleanup paths (8 scenarios)");
    return 0;
}
