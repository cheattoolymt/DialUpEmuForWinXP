/* SPDX-License-Identifier: BSD-2-Clause
 * XP-compatible event-driven WinMM output. The worker owns all WAVEHDRs and
 * their storage until reset/unprepare/close have returned ownership. */
#ifdef _WIN32
#include "vm_audio_internal.h"
#include <mmsystem.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum { BLOCKS = 3, BLOCK_MS = 20, MAX_SAMPLES = 1920 };
typedef struct {
    WAVEHDR header;
    int16_t pcm[MAX_SAMPLES];
    bool prepared, queued;
} wave_block_t;
typedef struct {
    HWAVEOUT output;
    HANDLE event;
    wave_block_t blocks[BLOCKS];
    float mix[MAX_SAMPLES];
} wave_impl_t;

static bool name_matches(const char *name, const char *match)
{
    size_t i, j;
    for (i = 0; name[i]; ++i) {
        for (j = 0; match[j]; ++j) {
            unsigned char a = (unsigned char)name[i+j];
            unsigned char b = (unsigned char)match[j];
            if (!a) break;
            if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
            if (a != b) break;
        }
        if (!match[j]) return true;
    }
    return false;
}

static vm_err_t select_device(vm_audio_t *a, UINT *device)
{
    UINT i;
    const char *match = a->params.device_match;
    *device = WAVE_MAPPER;
    snprintf(a->device_name, sizeof(a->device_name), "Default waveOut device");
    if (!match || !*match) return VM_OK;
    for (i = 0; i < waveOutGetNumDevs(); ++i) {
        WAVEOUTCAPSW caps;
        char name[VM_AUDIO_NAME_MAX];
        if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
        if (!WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1,
                                 name, sizeof(name), NULL, NULL)) continue;
        if (name_matches(name, match)) {
            *device = i;
            snprintf(a->device_name, sizeof(a->device_name), "%s", name);
            return VM_OK;
        }
    }
    return VM_ERR_NOTFOUND;
}

static bool reset_queue(vm_audio_t *a, wave_impl_t *w)
{
    int i;
    MMRESULT rc = waveOutReset(w->output);
    if (rc != MMSYSERR_NOERROR) {
        VM_LOGE("waveOutReset failed: %u", (unsigned)rc);
        return false;
    }
    for (i = 0; i < BLOCKS; ++i) w->blocks[i].queued = false;
    VM_ATOMIC_STORE_REL(&a->device_pending, 0);
    return true;
}

/* On a broken driver, retain its storage rather than freeing live DMA/header
 * memory. No function callback references a: notification uses only w->event.
 * The process releases retained resources on exit. Report this as a failure. */
static bool release_output(vm_audio_t *a, wave_impl_t *w)
{
    int i;
    if (w->output) {
        if (!reset_queue(a, w)) return false;
        for (i = 0; i < BLOCKS; ++i) {
            unsigned tries = 0;
            MMRESULT rc;
            if (!w->blocks[i].prepared) continue;
            do {
                rc = waveOutUnprepareHeader(w->output, &w->blocks[i].header, sizeof(WAVEHDR));
                if (rc == WAVERR_STILLPLAYING) {
                    (void)WaitForSingleObject(w->event, 5);
                }
            } while (rc == WAVERR_STILLPLAYING && ++tries < 200);
            if (rc != MMSYSERR_NOERROR) {
                VM_LOGE("waveOutUnprepareHeader failed: %u", (unsigned)rc);
                return false;
            }
            w->blocks[i].prepared = false;
        }
        if (waveOutClose(w->output) != MMSYSERR_NOERROR) return false;
    }
    if (w->event) CloseHandle(w->event);
    free(w);
    return true;
}

void vm_audio_run_waveout(vm_audio_t *a)
{
    static const struct { int rate, channels; } formats[] = {
        /* The modem already generates 8 kHz mono. Prefer that format so an
         * old CPU does not run a 96-tap FIR at 48 kHz merely to play a phone.
         * Fall back only when the driver rejects the requested format. */
        {8000, 1}, {8000, 2}, {11025, 1}, {22050, 1},
        {44100, 1}, {48000, 1}, {44100, 2}, {48000, 2}
    };
    wave_impl_t *w = calloc(1, sizeof(*w));
    vm_err_t error = VM_ERR_IO;
    UINT device;
    unsigned f;
    uint32_t frames = 0;
    int i, pending = 0;
    bool ready = false;
    if (!w) { error = VM_ERR_NOMEM; goto done; }
    w->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!w->event) goto done;
    error = select_device(a, &device);
    if (error != VM_OK) goto done;
    error = VM_ERR_IO;
    for (f = 0; f < sizeof(formats)/sizeof(formats[0]); ++f) {
        WAVEFORMATEX fmt;
        MMRESULT rc;
        memset(&fmt, 0, sizeof(fmt));
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = (WORD)formats[f].channels;
        fmt.nSamplesPerSec = (DWORD)formats[f].rate;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = (WORD)(fmt.nChannels * sizeof(int16_t));
        fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
        rc = waveOutOpen(&w->output, device, &fmt, (DWORD_PTR)w->event, 0, CALLBACK_EVENT);
        if (rc == MMSYSERR_NOERROR) {
            a->device_rate = formats[f].rate;
            a->device_channels = formats[f].channels;
            break;
        }
        w->output = NULL;
        /* Only retry another format for a format mismatch, not a missing,
         * busy or malfunctioning device. Never silently switch to null. */
        if (rc != WAVERR_BADFORMAT) goto done;
    }
    if (!w->output) goto done;
    frames = (uint32_t)a->device_rate * BLOCK_MS / 1000;
    if (frames * (uint32_t)a->device_channels > MAX_SAMPLES) goto done;
    error = vm_audio_backend_resampler_init(a);
    if (error != VM_OK) goto done;
    error = VM_ERR_IO;
    for (i = 0; i < BLOCKS; ++i) {
        WAVEHDR *h = &w->blocks[i].header;
        h->lpData = (LPSTR)w->blocks[i].pcm;
        h->dwBufferLength = frames * (DWORD)a->device_channels * sizeof(int16_t);
        if (waveOutPrepareHeader(w->output, h, sizeof(*h)) != MMSYSERR_NOERROR) goto done;
        w->blocks[i].prepared = true;
    }
    a->impl = w;
    a->ready_result = VM_OK;
    vm_ev_set(a->ready_ev);
    ready = true;
    while (!VM_ATOMIC_LOAD_ACQ(&a->quit)) {
        if (VM_ATOMIC_LOAD_ACQ(&a->flush_req)) {
            if (!reset_queue(a, w)) goto done;
            pending = 0;
            vm_audio_backend_handle_flush(a);
        }
        for (i = 0; i < BLOCKS; ++i) {
            wave_block_t *b = &w->blocks[i];
            uint32_t j, samples = frames * (uint32_t)a->device_channels;
            if (b->queued) {
                if (!(b->header.dwFlags & WHDR_DONE)) continue;
                b->queued = false;
                VM_ATOMIC_STORE_REL(&a->device_pending, --pending);
                vm_ev_set(a->space_ev);
            }
            if (VM_ATOMIC_LOAD_ACQ(&a->drain_req) && vm_frb_used(&a->ring) == 0) continue;
            /* Publish pending before consuming the ring, so drain cannot
             * return between consuming the final samples and waveOutWrite. */
            VM_ATOMIC_STORE_REL(&a->device_pending, ++pending);
            vm_audio_backend_render(a, w->mix, frames);
            for (j = 0; j < samples; ++j) {
                float sample = w->mix[j];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                b->pcm[j] = (int16_t)(sample * 32767.0f);
            }
            if (waveOutWrite(w->output, &b->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) goto done;
            b->queued = true;
        }
        (void)WaitForSingleObject(w->event, BLOCK_MS);
    }
    error = VM_OK;
done:
    if (w && !release_output(a, w)) {
        VM_LOGE("waveOut driver retained buffers; keeping their memory alive until process exit");
        error = VM_ERR_IO;
    }
    a->impl = NULL;
    if (!ready) {
        a->ready_result = error;
        vm_ev_set(a->ready_ev);
    } else if (error != VM_OK) {
        VM_LOGE("waveOut playback failed");
        VM_ATOMIC_STORE_REL(&a->failed, 1);
        vm_ev_set(a->space_ev);
    }
}
#endif
