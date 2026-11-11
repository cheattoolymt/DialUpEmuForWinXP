# Steps 3〜5: XP SP3 x86 診断版

## 到達点と対象範囲

XP用の本体ビルド、基本処理、waveOutバックエンドを実装した。
**コンパイル・静的検査とホスト試験は実施。XP上での実行・実音声・実COMは未確認。**
これはインターネット接続版ではない。Step 2のNDISドライバ検証とSteps 6〜8は残る。
起動時にも `Internet NAT and LAN/Wi-Fi isolation are NOT implemented` と表示する。

## Step 3: ビルドと依存関係

- 32bit x86、PE32、OS/subsystem 5.1を明示する。
- `VMODEM_TARGET_XP`、`WINVER=0x0501`、`_WIN32_WINNT=0x0501`、XP SP3のNTDDIを使う。
- 今回の検証コンパイラはDebianのMinGW-w64 GCC 14.2.0 / i686 / **win32 thread model**、headers 12.0.0。
- システム `msvcrt.dll` でビルドし、GCCランタイムを静的リンクする。UCRTや別配布のpthread DLLを必須にしない。
- 本体の直接依存DLLは `KERNEL32.dll`、`msvcrt.dll`、`WINMM.dll`、`WS2_32.dll` の4つ。
- WFP、WASAPI、libslirp/GLib、物理側ルート検出はXP診断版から除外する。
- 旧NDISドライバは選定・導入・同梱していない。後続のG1〜G5が完了するまで通信経路は実装しない。

参照: [MinGW配布とthread model](https://www.mingw-w64.org/downloads/)、
[MinGWのCRT選択資料](https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-doc/howto-build/ucrt-vs-msvcrt.txt)。
特定バージョンのコンパイラがすべてのXP環境を保証するという意味ではない。
実際の検証用パッケージとハッシュは [build-inputs.json](build-inputs.json) を参照。

LinuxのビルドホストにBash / rg / Python 3 / MinGWを用意する。XP側にこれらは不要。
Debianで用意するパッケージの例:

```text
gcc-mingw-w64-i686-win32
gcc-mingw-w64-i686-win32-runtime
mingw-w64-i686-dev
binutils-mingw-w64-i686
```

```bash
bash scripts/build-xp.sh
# 実行ファイル名やインストール場所が異なる場合
CC=/path/to/i686-w64-mingw32-gcc-win32 \
OBJDUMP=/path/to/i686-w64-mingw32-objdump bash scripts/build-xp.sh
```

生成物は `work/xp-build/vmodem-xp.exe`、`xp-smoke.exe`、`config-xp.ini`、各PE検査JSONとimport一覧。
ビルド時に既知の非対応API、予期しないDLL、PE64、5.1以外のtargetを検出すると失敗する。
これは静的な必要条件の検査であり、XPのloaderやすべてのAPI挙動を保証する完全検査ではない。
現行Windows版は既存の `scripts/build-windows-baseline.sh` でも引き続きビルドする。

## Step 4: 時計・I/O・待機

### 時計

XPでは `GetTickCount()` を使い、32bit差分を64bitに積算する。
サンプリングと積算をcritical sectionで直列化し、別スレッドの古いサンプルを
折り返しと誤認しないようにした。利用者は49.7日以内ごとにサンプリングする必要がある。
通常のモデムループは継続して時計を読む。49.7日を超えるスリープ等での複数周回は判別できない。
時刻調整に依存せず、精度はOSのtick分解能 (通常10〜16ms) に従う。

QPCはXPにも存在するが、一部の古いBIOS/複数CPUでの問題がMicrosoftに記載されているため、
XP診断版はQPCを必須にしない。
[GetTickCountの分解能と折り返し](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount)、
[QPCのXPに関する注意](https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps)。

### COMの取消し

XP側は `CancelIo()` を使い、取消し後の `GetOverlappedResult(..., TRUE)` を維持する。
COMのread/write/closeは同じモデムイベントループスレッドで行う。
read/writeのラッパは保留I/Oを完了または取消ししてから戻り、同一handleで独立したread/writeを
並列発行しない。コンソールハンドラは停止要求だけを出す。
別スレッドから任意のI/Oを取り消せるAPIに置き換えたわけではない。
[CancelIoのスレッド制約と完了確認](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelio)。

### ネットワーク待機の境界

XPの現段階では `none` と `loopback` だけを明示的に許可する。
`slirp` / `ndis` / 不明なモードはCOMを開く前にエラーにする。
`WSAPoll`を呼ぶlibslirpバックエンドはXPビルドへリンクしない。
これにより「WSAPollの代わりにselectが実装済み」とはしていない。
将来libslirpをXPで有効にするなら別途ソケット監視と送信経路の実装が必要。
NDISバックエンドのイベント待機も後続工程で実装する。

## Step 5: waveOut

XPターゲットではAUTO音声バックエンドをwaveOutへ切り替える。現行WindowsのAUTOはWASAPIのまま。
PCM16 / 20ms × 3バッファ。既存DSP、リングバッファ、リサンプラ、音量処理を通して再生する。

- 48kHz stereo → 44.1kHz stereo → 22.05kHz mono → 11.025kHz mono → 8kHz mono の順に試す。
  形式不一致の場合だけ次へ進み、busy/no-driver等を別形式の問題として隠さない。
- 既定のwave mapper、またはUTF-8デバイス名の部分一致で選択できる。
  大文字小文字の同一視はASCIIのみ。指定したデバイスがなければエラー。
- `CALLBACK_EVENT`で完了通知を受け、`WHDR_DONE`のバッファだけを再利用する。
- drainはリングだけでなくデバイスに渡したバッファの完了も待つ。
- flushは `waveOutReset()` でキューを戻してからリング/リサンプラをクリアする。
- 終了時はreset→unprepare→closeの順で、ドライバが保持中のheader/PCMを解放しない。
- 再生中のエラーはフラグとログへ出し、モデムは無音でシーケンスを進める。
  `--audio-test`は初期化/再生エラーなら失敗を返す。
- 壊れたドライバがreset/unprepareに失敗する場合、使用中メモリは意図的に保持してログへ記録する。
  その場合の最終解放はプロセス終了に委ね、正常復旧できたとは扱わない。
  workerの終了前に本体のリング等を解放しない。ドライバ呼出し自体が停止した場合の
  終了時間を保証するものではない。

参照: [waveOutOpenとevent通知](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutopen)、
[waveOutWriteとWHDR_DONE](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutwrite)、
[waveOutReset](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutreset)、
[waveOutUnprepareHeaderとバッファ寿命](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/nf-mmeapi-waveoutunprepareheader)。

## XPでの確認手順 (未実行)

生成した2つのEXEと `config-xp.ini` を同じフォルダへ置き、そこでコマンドプロンプトを開く。

```bat
xp-smoke.exe
vmodem-xp.exe --audio-test
vmodem-xp.exe -c config-xp.ini -p CNCB0
```

1. `xp-smoke.exe`: loader、時計、Winsock、AT解釈器、メモリ上のserial loopbackを試す。
   さらにoverlapped named pipeで、保留readのtimeout/CancelIo/完了待ち→再度read成功を20回試す。
   これは実COMドライバの検証ではない。失敗した行番号を保存する。
2. `--audio-test`: COMを開かず400Hzを約1秒再生する。終了コードとエラーログ、実際の聞こえ方を確認。
3. 実COMはcom0com等を別途用意する。対向側から `AT` / `ATE0` / `ATZ` を送り応答を確認。
   発信・途中中断・Ctrl-C、COM使用中・不存在、音声デバイス不存在も試す。
4. RASと接続できても、現段階でインターネットには出ない。LAN/Wi-Fiの直接利用も制限しない。
   ネットワーク専用化の受入れ試験は後続のG1〜G5で行う。

XP診断版は `config-xp.ini` を既定で読み、読めなければ終了する。
`mode=none` は意図した試験設定であり、NAT失敗時の自動fallbackではない。
`--net loopback` は内部ICMP/UDP応答の試験専用である。

## ホスト試験と証拠

```bash
python3 scripts/check-baseline.py
bash scripts/test-xp-host.sh
bash scripts/test-baseline.sh --slirp
```

`check-baseline.py` のGitオブジェクト検証は引き続き通る。改造したので `--worktree` が
元67ファイルとの不一致を報告するのは正常。基準タグ・manifestは更新しない。

ホスト試験のwaveOutは **実装したworkerをデバイス模型で動かす試験**。
PCM変換、drain/flush、形式・名前選択、prepare/write/reset/unprepareの失敗と後始末を確認する。
XPの音声ドライバ、WinMMの実イベント順、物理デバイスの遅延を実証するものではない。
結果は [03-05-results.json](03-05-results.json) に、XP未実行という制限とともに記録する。
