/* Minimal host-test seam, never included by a production Windows build. */
#ifndef FAKE_WINDOWS_H
#define FAKE_WINDOWS_H
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>
typedef void *HANDLE;
typedef uint32_t DWORD;
typedef uintptr_t DWORD_PTR;
typedef uint16_t WORD;
typedef unsigned UINT;
typedef int BOOL;
typedef char *LPSTR;
#define FALSE 0
#define CP_UTF8 65001
HANDLE CreateEventW(void *, BOOL, BOOL, const wchar_t *);
BOOL CloseHandle(HANDLE);
DWORD WaitForSingleObject(HANDLE, DWORD);
int WideCharToMultiByte(UINT, DWORD, const wchar_t *, int, char *, int, const char *, BOOL *);
#endif
