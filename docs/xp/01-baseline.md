# Step 1: 改造前の基準点

記録日: 2026-11-11 (JST)

## 固定した内容

| 項目 | 値 |
|---|---|
| 専用リポジトリ | `cheattoolymt/DialUpEmuForWinXP` |
| 基準コミット | `a70af1fb819eed91033c452192f11f4c7dd52524` |
| 基準タグ | `xp-baseline-20261111` (軽量タグ。以後付け替えない) |
| Git tree | `cd03b47cc29c3784fd738947b8bfb70cd2299f8e` |
| コピー元 | `cheattoolymt/WinDialupEmu` の `0600952f8d02249cb6db830be764cdc0e7224cc4` |
| コピー対象 | 67 ファイル、920,591 bytes。Git 管理対象のみ |

全ファイルのサイズと SHA-256 は [baseline-manifest.json](baseline-manifest.json) に保存。
今回追加する文書・検証スクリプトを除き、基準点の 67 ファイルは変更しない。
コピー元リポジトリも変更しない。

## 再現手順

以下は Linux 等の検証ホストで実行する。XP 本体に Bash/Python/rg を要求するものではない。
必要なもの: Git、Bash、ripgrep (`rg`)、Python 3、C99 コンパイラ、POSIX threads。

```bash
# Git オブジェクトの tree / 全67ファイルのSHA-256を確認
python3 scripts/check-baseline.py
# さらに、コピーされた全ファイルが作業ツリーでも未変更か確認
python3 scripts/check-baseline.py --worktree

# libslirpなしで既存8スイート
bash scripts/test-baseline.sh

# libslirpを用意した環境でUDP/TCP統合試験も実行
bash scripts/test-baseline.sh --slirp
# 通常のリンカ探索対象外にライブラリがある場合
SLIRP_LIBS=/absolute/path/libslirp.so.0 bash scripts/test-baseline.sh --slirp
```

結果のログ・JSON・テスト生成音声は `work/baseline-tests/` に置く。
実行ファイルと音声の生成は試験用であり、GitHub へはアップロードしない。
`SLIRP_LIBS` は空白で区切ったリンカ引数。シェルコマンドとして評価しない。

コピー元の Windows x64 ビルドの再現:

```bash
CC=x86_64-w64-mingw32-gcc \
OBJDUMP=x86_64-w64-mingw32-objdump \
SLIRP_LIBDIR=/path/to/mingw64/lib \
bash scripts/build-windows-baseline.sh
```

MinGW-w64 と、対応する libslirp のインポートライブラリが必要。
生成物は `work/windows-baseline/vmodem.exe` と `imports.txt`。
これは **元の Windows x64 版を再現する手順**。XP 用のビルド手順ではない。
ソース内の全警告を `-Wall -Wextra -Werror` でエラー扱いにする。

## 検証範囲

今回の結果は [baseline-results.json](baseline-results.json) を参照。
既存8スイート合計371項目、実libslirpのUDP/TCP往復、Windows x64向けコンパイル・リンクを確認する。
Linux 上の回帰テストは XP の RAS / COM / 音声 / NDIS / WFP を実行していない。
Windows 向けリンク成功も Windows 上での起動・疎通成功を意味しない。

## 戻す・比較する方法

開発途中の変更を上書きせず基準点を確認するには、別作業ツリーを作る。

```bash
git worktree add --detach ../DialUpEmuForWinXP-baseline xp-baseline-20261111
git diff xp-baseline-20261111 -- src include config.ini README.md
```

今後の実装で `--worktree` の比較が失敗するのは意図した変更なら正常。
manifest の基準ハッシュを更新して失敗を隠さず、既存 Git オブジェクトの検証と回帰試験を続ける。
