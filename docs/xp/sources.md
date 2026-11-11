# Step 2 の Web 調査記録

確認日: 2026-11-11 (JST)。一次資料のみを設計の根拠とする。
検索スニペットや、最新製品の説明をそのまま XP 旧版の保証に置き換えない。
「資料で確認」「設計判断」「実機未確認」は別の状態として記録する。
以下は短い要約であり、文書や第三者のコードの転載ではない。

| ID | 一次資料 | 確認したこと |
|---|---|---|
| S01 | [Microsoft: WFPへの移行](https://learn.microsoft.com/en-us/windows-hardware/drivers/network/porting-packet-processing-drivers-and-apps-to-wfp) | XP/2003の手段としてTDI、LSP、NDIS中間ドライバ等を列挙。Vista以降はWFPへ移行する位置付け |
| S02 | [Microsoft: IPPROTO_IP socket options](https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options) | OS対応表のXP/2003列では `IP_UNICAST_IF` が空欄。Vista列は対応。`IP_MULTICAST_IF` はユニキャストの代替ではない |
| S03 | [Microsoft: WSAPoll](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll) | Vista以降のAPI。Winsock 2.2を要求するだけではXPで使えるようにならない |
| S04 | [Microsoft: CancelIoEx](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex) | Vista以降。取消し要求後もOVERLAPPEDの寿命と完了確認が必要 |
| S05 | [Microsoft: CancelIo](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelio) | XP対応。同じスレッドが発行したI/Oだけを取り消す |
| S06 | [Microsoft: GetTickCount64](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-gettickcount64) | Vista以降。XPの代替時計で折り返しを扱う必要がある |
| S07 | [Microsoft: Core Audio APIs](https://learn.microsoft.com/en-us/windows/win32/coreaudio/about-the-windows-core-audio-apis) | WASAPIを含むCore AudioはVista導入。XPへ提供しないことも明記 |
| S08 | [NT KERNEL: legacy download](https://www.ntkernel.com/windows-packet-filter/download/) | v3.6.1アーカイブ内の別インストーラにXP/2003等向け旧ドライバがある。現在のNDIS6 MSIとは別物 |
| S09 | [NT KERNEL: SetAdapterMode C API](https://www.ntkernel.com/docs/windows-packet-filter-documentation/ndisapi-c-2/setadaptermode/) | SENT/RECVのTUNNELは元パケットを止めてキューへ渡す。LISTENは原本を通すため排他制御には不足 |
| S10 | [NT KERNEL: SendPacketToAdapter C API](https://www.ntkernel.com/docs/windows-packet-filter-documentation/ndisapi-c-2/sendpackettoadapter/) | Ethernetフレームと明示的なアダプタハンドルで送出できる。旧NDISでは一部flagsが機能しない注意もある |
| S11 | [NT KERNEL: Internet Gateway](https://www.ntkernel.com/windows-packet-filter/internet-gateway/) | XPを含む対応表と、ユーザモードのTCP/UDP NATサンプルが存在。今回の同一PC RAS構成の実証ではない |
| S12 | [NT KERNEL: licensing](https://www.ntkernel.com/windows-packet-filter/licensing/) | ドライバとSDKの利用・商用配布条件がある。公開SDKを見られることはBSDでドライバを再配布できる根拠にならない |
| S13 | [Microsoft: IRP_MJ_CLEANUP](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/irp-mj-cleanup) | 最後のfile-objectハンドルが閉じた際の後始末をドライバが実装する必要がある |
| S14 | [Microsoft: process termination](https://learn.microsoft.com/en-us/windows/win32/procthread/terminating-a-process) | プロセス終了時にハンドルは閉じる。ただしサードパーティドライバのfilter復元までは保証しない |

| S15 | [NT KERNEL公式GitHub: v3.6.1 release](https://github.com/wiresock/ndisapi/releases/tag/v3.6.1) | legacy installerとXP/2003以降向けVS2012基本サンプルへのリンクを確認。パッケージの展開・実行・内部API確認は未実施 |

## 根拠から導ける範囲

- S01/S02から、既存WFP＋`IP_UNICAST_IF`経路の単純なXP移植は採用しない。
- S09/S10から、ホストTCP/IPの通常送受信を止めつつアダプタを指定して送出する設計を候補にできる。
- S08/S11は、XP向け旧実装を調べる具体的な入口になる。新APIが全て旧版にあるとは判断しない。
- S13/S14だけで「強制終了すればWinpkFilter旧版も必ず復元する」とは判断しない。
- S12は現在の条件の確認。実際に採用する旧パッケージの同梱条件を、版・ファイル単位で再確認する。

## 現時点で確認できていないこと

1. 旧XP版の具体的なDLL/SYS/INFの版・ハッシュと、個々のAPIの存在。
2. 同一XP上のPPP→NDIS→NICの往復、および注入フレームの再捕捉挙動。
3. 旧ドライバのハンドル終了時・アプリ異常終了時・ハング時の復元。
4. 対象のWi-Fiカード、再接続、802.1X/EAPOL、複数NIC、追加NICを含む挙動。
5. 配布対象となる旧ドライバの再配布条件。

これらは検索で成功扱いにせず、[検証ゲート](validation-plan.md)で確認する。
今回、ドライバの購入・ベンダーへの連絡・第三者バイナリの同梱は行っていない。
