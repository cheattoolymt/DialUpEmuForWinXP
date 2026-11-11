# Windows XP 専用版の改造記録

対象は **Windows XP SP3 / x86 (32bit)**。XP 上で RAS とエミュレータ本体を動かす。
Steps 3〜5のXP用診断ビルドを実装したが、**XP実機での動作確認は未実施**。
インターネット接続とLAN専用化はまだ実装していない。

1. [基準点・再現手順・検証結果](docs/xp/01-baseline.md)
2. [ネットワーク方式の設計判断](docs/xp/02-network-design.md)
3. [Web 調査の出典と確認事項](docs/xp/sources.md)
4. [実機検証ゲートと続きの工程](docs/xp/validation-plan.md)
5. [Steps 3〜5の実装・ビルド・実機確認手順](docs/xp/03-05-implementation.md)

基準タグ: [`xp-baseline-20261111`](https://github.com/cheattoolymt/DialUpEmuForWinXP/tree/xp-baseline-20261111)

現在の到達点はStep 1の基準点固定、Step 2の資料調査・方式選定と、Steps 3〜5の診断版実装。
XPドライバ導入、パケット送受信PoC、実機実行は未実施。
Step 2 で選んだ方式は設計上の選定であり、実機で成立を確認したという意味ではない。
