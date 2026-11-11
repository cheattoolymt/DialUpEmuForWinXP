#ifndef FAKE_MMSYSTEM_H
#define FAKE_MMSYSTEM_H
#include "windows.h"
typedef void *HWAVEOUT;
typedef UINT MMRESULT;
typedef struct {
    LPSTR lpData;
    DWORD dwBufferLength, dwBytesRecorded;
    DWORD_PTR dwUser;
    DWORD dwFlags, dwLoops;
    void *lpNext;
    DWORD_PTR reserved;
} WAVEHDR;
typedef struct {
    WORD wFormatTag, nChannels;
    DWORD nSamplesPerSec, nAvgBytesPerSec;
    WORD nBlockAlign, wBitsPerSample, cbSize;
} WAVEFORMATEX;
typedef struct { wchar_t szPname[32]; } WAVEOUTCAPSW;
#define WAVE_MAPPER ((UINT)-1)
#define WAVE_FORMAT_PCM 1
#define CALLBACK_EVENT 0x50000
#define WHDR_DONE 1
#define WHDR_PREPARED 2
#define WHDR_INQUEUE 16
#define MMSYSERR_NOERROR 0
#define MMSYSERR_ERROR 1
#define WAVERR_BADFORMAT 32
#define WAVERR_STILLPLAYING 33
MMRESULT waveOutOpen(HWAVEOUT *, UINT, const WAVEFORMATEX *, DWORD_PTR, DWORD_PTR, DWORD);
MMRESULT waveOutClose(HWAVEOUT);
MMRESULT waveOutPrepareHeader(HWAVEOUT, WAVEHDR *, UINT);
MMRESULT waveOutUnprepareHeader(HWAVEOUT, WAVEHDR *, UINT);
MMRESULT waveOutWrite(HWAVEOUT, WAVEHDR *, UINT);
MMRESULT waveOutReset(HWAVEOUT);
UINT waveOutGetNumDevs(void);
MMRESULT waveOutGetDevCapsW(UINT, WAVEOUTCAPSW *, UINT);
#endif
