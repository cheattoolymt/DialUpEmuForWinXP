# Windows XP 専用版の改造記録

対象は **Windows XP SP3 / x86 (32bit)**。XP 上で RAS とエミュレータ本体を動かす。
現在の本体はコピー元の Windows 10/11 版のままであり、**XP 対応済みではない**。

1. [基準点・再現手順・検証結果](docs/xp/01-baseline.md)
2. [ネットワーク方式の設計判断](docs/xp/02-network-design.md)
3. [Web 調査の出典と確認事項](docs/xp/sources.md)
4. [実機検証ゲートと続きの工程](docs/xp/validation-plan.md)

基準タグ: [`xp-baseline-20261111`](https://github.com/cheattoolymt/DialUpEmuForWinXP/tree/xp-baseline-20261111)

今回の到達点は Step 1 の基準点固定と Step 2 の資料調査・方式選定。
XP のドライバ導入、パケット送受信 PoC、本体移植は実施していない。
Step 2 で選んだ方式は設計上の選定であり、実機で成立を確認したという意味ではない。
