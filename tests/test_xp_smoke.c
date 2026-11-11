/* SPDX-License-Identifier: BSD-2-Clause
 * Run on XP without COM hardware. Does not validate real COM or waveOut. */
#include <stdio.h>
#include <string.h>
#include "vmodem/vm_winsock.h"
#include "vmodem/vm_clock.h"
#include "vmodem/vm_at.h"
#include "vmodem/vm_config.h"
#include "vmodem/vm_serial.h"
#include "vmodem/vm_log.h"
#include "vm_serial_internal.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
/* Exercise the real serial read wrapper's pending-I/O cancellation using an
 * overlapped named pipe. Unlike the COM immediate-read mode, an empty pipe
 * deterministically enters ERROR_IO_PENDING. No virtual COM driver required. */
static int pending_io_test(void)
{
    vm_serial_t s;
    HANDLE client = INVALID_HANDLE_VALUE;
    OVERLAPPED connected;
    char name[128], data[8];
    DWORD transferred;
    BOOL connect_pending = FALSE;
    int i, result = 1;
#define PIPE_CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL pipe line %d: %s\n", __LINE__, #x); goto cleanup; } } while (0)
    memset(&s, 0, sizeof(s));
    memset(&connected, 0, sizeof(connected));
    snprintf(name, sizeof(name), "\\\\.\\pipe\\vmodem-xp-test-%lu", (unsigned long)GetCurrentProcessId());
    s.h = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                          PIPE_TYPE_BYTE | PIPE_WAIT, 1, 1024, 1024, 0, NULL);
    PIPE_CHECK(s.h != INVALID_HANDLE_VALUE);
    connected.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    PIPE_CHECK(connected.hEvent != NULL);
    PIPE_CHECK(!ConnectNamedPipe(s.h, &connected) && GetLastError() == ERROR_IO_PENDING);
    connect_pending = TRUE;
    client = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    PIPE_CHECK(client != INVALID_HANDLE_VALUE);
    PIPE_CHECK(GetOverlappedResult(s.h, &connected, &transferred, TRUE));
    connect_pending = FALSE;
    s.ov_read_ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    s.cancel_ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    PIPE_CHECK(s.ov_read_ev && s.cancel_ev);
    for (i=0; i<20; ++i) {
        PIPE_CHECK(vm_serial_read_win32(&s, data, sizeof(data), 5) == 0);
        PIPE_CHECK(!s.read_pending);
        PIPE_CHECK(WriteFile(client, "AT\r", 3, &transferred, NULL) && transferred == 3);
        PIPE_CHECK(vm_serial_read_win32(&s, data, sizeof(data), 100) == 3);
        PIPE_CHECK(memcmp(data, "AT\r", 3) == 0);
    }
    result = 0;
cleanup:
    if (s.h && s.h != INVALID_HANDLE_VALUE) {
        CancelIo(s.h);
        if (connect_pending) GetOverlappedResult(s.h, &connected, &transferred, TRUE);
        if (s.read_pending) GetOverlappedResult(s.h, &s.ov_read, &transferred, TRUE);
        CloseHandle(s.h);
    }
    if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
    if (connected.hEvent) CloseHandle(connected.hEvent);
    if (s.ov_read_ev) CloseHandle(s.ov_read_ev);
    if (s.cancel_ev) CloseHandle(s.cancel_ev);
#undef PIPE_CHECK
    return result;
}

int main(void)
{
    vm_config_t cfg;
    vm_at_t at;
    vm_serial_t *serial = NULL;
    vm_serial_params_t params;
    char buf[256], error[256];
    int count;
    uint64_t before;
    vm_log_init(VM_LOG_ERROR, NULL);
    CHECK(vm_winsock_init(error, sizeof(error)) == 0);
    before = vm_clock_now_ms64();
    Sleep(30);
    CHECK(vm_clock_now_ms64() >= before + 1);
    vm_config_defaults(&cfg);
    vm_at_init(&at, &cfg);
    vm_at_feed(&at, "ATE0\r", 5);
    count = vm_at_pop_response(&at, (uint8_t *)buf, sizeof(buf)-1);
    CHECK(count > 0);
    buf[count] = 0;
    CHECK(strstr(buf, "OK") != NULL);
    vm_serial_params_defaults(&params);
    params.backend = VM_SERIAL_BACKEND_LOOPBACK;
    CHECK(vm_serial_open(&serial, &params, error, sizeof(error)) == VM_OK);
    CHECK(vm_serial_peer_write(serial, "AT\r", 3) == 3);
    CHECK(vm_serial_read(serial, buf, sizeof(buf), 0) == 3);
    CHECK(memcmp(buf, "AT\r", 3) == 0);
    vm_serial_close(serial);
    CHECK(pending_io_test() == 0);
    vm_winsock_cleanup();
    puts("PASS: loader, clock, Winsock, AT and in-memory serial loopback and pending-I/O cancellation");
    return 0;
}
