/*
 * main.c - VModem エントリポイント
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * やることは 4 つだけ。
 *   1. config.ini を読む
 *   2. ログを初期化する
 *   3. Ctrl-C ハンドラを設置する
 *   4. vm_modem_run() を呼ぶ
 *
 * ★ Ctrl-C の扱いについて ★
 * Windows の SetConsoleCtrlHandler は **別スレッド** でハンドラを呼ぶ。
 * POSIX の signal はシグナルコンテキストで呼ぶ。どちらの場合も
 * 「そこで後片付けをする」のは危険 (COM を閉じている最中に
 *  イベントループが同じハンドルを触る)。
 * したがってハンドラでは vm_modem_request_stop() でフラグを立てるだけにし、
 * 実際の後片付けはイベントループが自分で抜けてから main が行う。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ★順序厳守★
 * vm_winsock.h は winsock2.h -> ws2tcpip.h -> windows.h の順を保証する。
 * windows.h を先に通すと古い winsock.h (Winsock 1.1) が入り、
 * 後から winsock2.h を読んだ翻訳単位と型が食い違う。
 * したがって **他のどのヘッダより先に** これを置く。
 */
#include "vmodem/vm_winsock.h"

#include "vmodem/vm_modem.h"
#include "vmodem/vm_config.h"
#include "vmodem/vm_log.h"
#include "vmodem/vm_nat.h"
#include "vmodem/vm_audio.h"
#include "vmodem/vm_clock.h"
#include "vmodem/vm_sequence.h"
#include <math.h>

#ifndef _WIN32
#  include <signal.h>
#endif

static vm_modem_t *g_modem = NULL;

#ifdef _WIN32
static BOOL WINAPI ctrl_handler(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        fprintf(stderr, "\n終了要求を受けました。切断処理をします...\n");
        vm_modem_request_stop(g_modem);
        return TRUE;
    default:
        return FALSE;
    }
}
#else
static void on_signal(int sig)
{
    (void)sig;
    vm_modem_request_stop(g_modem);
}
#endif

/* Audible diagnostic without COM, RAS or network changes. */
static int audio_test(bool dial)
{
    vm_audio_t *audio = NULL;
    vm_audio_params_t params;
    float block[160];
    unsigned sample = 0, part;
    vm_sequence_t sequence;
    vm_config_t config;
    uint64_t started, render_ms = 0, max_render_ms = 0;
    bool done = false;
    vm_audio_params_defaults(&params);
    vm_log_init(VM_LOG_INFO, NULL);
    if (dial) {
        vm_config_defaults(&config);
        if (vm_sequence_start_dial(&sequence, "0120-000-0000", NULL,
                &config, VM_STD_V34PLUS, 33600, 8000, 1.0f) != VM_OK)
            return 1;
    }
    if (vm_audio_open(&audio, &params) != VM_OK) return 1;
    started = vm_clock_now_ms64();
    for (part = 0; part < (dial ? 6000u : 50u); ++part) {
        unsigned i, offset = 0;
        uint64_t deadline = vm_clock_now_ms64() + 3000;
        uint64_t render_start = vm_clock_now_ms64();
        if (dial) {
            done = vm_sequence_render(&sequence, block, 160);
            sample += 160;
        } else {
            for (i = 0; i < 160; ++i, ++sample)
                block[i] = 0.3f * (float)sin(6.283185307179586 * 400 * sample / 8000);
        }
        {
            uint64_t elapsed = vm_clock_now_ms64() - render_start;
            render_ms += elapsed;
            if (elapsed > max_render_ms) max_render_ms = elapsed;
        }
        while (offset < 160) {
            offset += vm_audio_write(audio, block + offset, 160 - offset);
            if (vm_audio_failed(audio) || vm_clock_now_ms64() >= deadline) {
                vm_audio_close(audio);
                return 1;
            }
            if (offset < 160) vm_audio_wait_drain(audio, 1, 5);
        }
        if (dial && done) break;
    }
    vm_audio_drain(audio, 3000);
    part = vm_audio_failed(audio) || !vm_audio_is_drained(audio) || (dial && !done);
    VM_LOGI("audio-test: mode=%s expected_ms=%u elapsed_ms=%llu render_ms=%llu max_block_ms=%llu missing_frames=%u empty_frames=%u drained=%d",
            dial ? "dial" : "tone", sample / 8,
            (unsigned long long)(vm_clock_now_ms64() - started),
            (unsigned long long)render_ms, (unsigned long long)max_render_ms,
            vm_audio_underruns(audio), vm_audio_idle_frames(audio),
            vm_audio_is_drained(audio) ? 1 : 0);
    vm_audio_close(audio);
    return part ? 1 : 0;
}

static void usage(const char *argv0)
{
    printf(
"VModem - Windows 用 本物のダイアルアップモデムエミュレータ\n"
"\n"
"使い方: %s [オプション]\n"
"\n"
#ifdef VMODEM_TARGET_XP
"  -c, --config <path>   Config file (default: config-xp.ini)\n"
#else
"  -c, --config <path>   設定ファイル (既定: config.ini)\n"
#endif
"  -p, --port <name>     COM ポート名を上書き (例: CNCB0)\n"
"  -v, --verbose         ログレベルを DEBUG に\n"
"      --trace           ログレベルを TRACE に (バイト単位のダンプ)\n"
"  -q, --quiet           音を出さない\n"
"      --wav <path>      合成音を WAV に書き出す (デバッグ用)\n"
#ifdef VMODEM_TARGET_XP
"      --net <mode>      none | loopback (diagnostic only; no Internet/isolation)\n"
#else
"      --net <mode>      slirp | loopback | none\n"
#endif
"      --audio-test      Play a 400 Hz tone without COM/network access\n"
"      --dial-audio-test Play the default V.34+ dialing sequence without COM\n"
"  -h, --help            このヘルプ\n"
"\n"
"設定例 (config.ini):\n"
"  [ISP_1]\n"
"  number   = 0120-000-0000\n"
"  speed    = 33600\n"
"  protocol = V34PLUS\n"
"\n", argv0);
}

int main(int argc, char **argv)
{
#ifdef VMODEM_TARGET_XP
    const char *cfg_path = "config-xp.ini";
#else
    const char *cfg_path = "config.ini";
#endif
    const char *port_override = NULL;
    const char *net_override  = NULL;
    const char *wav_override  = NULL;
    int  level_override = -1;
    int  quiet = 0;
    int  test_audio = 0;
    vm_config_t cfg;
    char errbuf[256];
    char status[256];
    vm_err_t rc;
    int i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 0; }
        else if ((!strcmp(a, "-c") || !strcmp(a, "--config")) && i + 1 < argc)
            cfg_path = argv[++i];
        else if ((!strcmp(a, "-p") || !strcmp(a, "--port")) && i + 1 < argc)
            port_override = argv[++i];
        else if (!strcmp(a, "--net") && i + 1 < argc)
            net_override = argv[++i];
        else if (!strcmp(a, "--wav") && i + 1 < argc)
            wav_override = argv[++i];
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose"))
            level_override = VM_LOG_DEBUG;
        else if (!strcmp(a, "--trace"))
            level_override = VM_LOG_TRACE;
        else if (!strcmp(a, "--audio-test"))
            test_audio = 1;
        else if (!strcmp(a, "--dial-audio-test"))
            test_audio = 2;
        else if (!strcmp(a, "-q") || !strcmp(a, "--quiet"))
            quiet = 1;
        else {
            fprintf(stderr, "不明なオプション: %s\n", a);
            usage(argv[0]);
            return 2;
        }
    }

    if (test_audio) return audio_test(test_audio == 2);

    /* --- 設定 --- */
    vm_config_defaults(&cfg);
    errbuf[0] = '\0';
    rc = vm_config_load(&cfg, cfg_path, errbuf, sizeof(errbuf));
    if (rc != VM_OK) {
#ifdef VMODEM_TARGET_XP
        fprintf(stderr, "Cannot load XP diagnostic config '%s': %s\n", cfg_path, errbuf);
        return 1;
#endif
        /*
         * 設定が読めなくても既定値で起動できるようにする。
         * 「まず動かして音を聞きたい」というのが最初の使い方なので、
         * ここで終了するとハードルが上がる。
         */
        fprintf(stderr,
                "警告: '%s' を読み込めませんでした (%s)。既定値で起動します。\n",
                cfg_path, errbuf[0] ? errbuf : "理由不明");
    }

    if (port_override != NULL)
        snprintf(cfg.com_port, sizeof(cfg.com_port), "%s", port_override);
    if (net_override != NULL)
        snprintf(cfg.net_mode, sizeof(cfg.net_mode), "%s", net_override);
    if (wav_override != NULL) {
        cfg.dump_wav = true;
        snprintf(cfg.dump_wav_path, sizeof(cfg.dump_wav_path), "%s", wav_override);
    }
    if (quiet) cfg.audio_enable = false;
    if (level_override >= 0) cfg.log_level = level_override;

    /* --- ログ --- */
    vm_log_init((vm_log_level_t)cfg.log_level,
                cfg.log_file[0] ? cfg.log_file : NULL);

#ifdef VMODEM_TARGET_XP
    VM_LOGW("XP Steps 3-5 DIAGNOSTIC: Internet NAT and LAN/Wi-Fi isolation are NOT implemented");
#endif

    /* --- Winsock ---
     *
     * ★必ず vm_modem_create() より前★
     *
     * Windows では WSAStartup() を呼ぶまで socket() / sendto() /
     * WSAPoll() が WSANOTINITIALISED (10093) で全部失敗する。
     *
     * libslirp も内部で WSAStartup(MAKEWORD(2, 0)) を呼ぶが、
     *   - 要求バージョンが 2.0 で、WSAPoll は 2.2 の API
     *   - atexit() で WSACleanup() する
     * ため、我々が先に 2.2 で初期化し、自分の参照カウントを持つ。
     * 詳細は include/vmodem/vm_winsock.h 冒頭「罠 2」。
     *
     * ログ初期化より後に置いているのは、失敗理由をログに残すため。
     */
    errbuf[0] = '\0';
    if (vm_winsock_init(errbuf, sizeof(errbuf)) != 0) {
        fprintf(stderr,
                "エラー: Winsock を初期化できませんでした (%s)\n",
                errbuf[0] ? errbuf : "理由不明");
        VM_LOGE("winsock: 初期化失敗 (%s)", errbuf[0] ? errbuf : "理由不明");
        return 1;
    }

    VM_LOGI("VModem 起動 (config=%s port=%s net=%s isp=%d 件)",
            cfg_path, cfg.com_port, cfg.net_mode, cfg.isp_count);
    for (i = 0; i < cfg.isp_count; i++) {
        VM_LOGI("  [%s] %s (正規化 %s) %d bps %s",
                cfg.isp[i].section, cfg.isp[i].number_raw,
                cfg.isp[i].number_norm, cfg.isp[i].speed,
                vm_standard_name(cfg.isp[i].protocol));
    }
    if (cfg.isp_count == 0)
        VM_LOGW("ISP エントリが 0 件です。どの番号にダイアルしても "
                "NO CARRIER になります");

    /* --- モデム --- */
    rc = vm_modem_create(&g_modem, &cfg);
    if (rc != VM_OK) {
        fprintf(stderr,
                "エラー: モデムを初期化できませんでした (%s)\n"
                "COM ポート '%s' が存在し、他のプロセスが開いていないか\n"
                "確認してください。上のログで NAT / WFP の失敗が出ている場合は\n"
                "LAN / Wi-Fi、libslirp、および管理者権限も確認してください。\n",
                vm_strerror(rc), cfg.com_port);
        vm_winsock_cleanup();
        return 1;
    }

    /* --- Ctrl-C --- */
#ifdef _WIN32
    SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
#endif

    printf("VModem 待機中。Windows のダイアルアップ接続から発信してください。\n"
           "終了は Ctrl-C。\n");
    fflush(stdout);

    rc = vm_modem_run(g_modem);

    VM_LOGI("最終状態: %s", vm_modem_status(g_modem, status, sizeof(status)));
    vm_modem_destroy(g_modem);
    g_modem = NULL;

    vm_winsock_cleanup();

    return (rc == VM_OK) ? 0 : 1;
}
