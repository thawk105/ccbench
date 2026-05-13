# Coding conventions

このリポジトリ固有のコーディング規約と、ファイル種別ごとに **指摘されなくても**
適用すべき業界 best practice をまとめたもの。

**運用方針**:

- ファイルを編集する前に、その種別の節を一度確認する
- 規約に反する既存コードを見つけても、**スコープを膨らませず別 PR** で揃える
- 新しい知見が出たらこのドキュメントに追記する (Claude / contributors どちらでも)

最初は薄い。気付いたものから追記していくこと。

---

## Dockerfile

- **`apt-get install` の multi-line args は alphabetical sort**
  ([Docker docs: Sort multi-line arguments](https://docs.docker.com/build/building/best-practices/#sort-multi-line-arguments))。
  重複追加防止 + merge conflict 低減 + レビュー性向上のため。
- **`--no-install-recommends` を必ず付ける**。recommends で余計な dev tool が
  入ると image が膨らみ、CI の container init が遅くなる。
- **BuildKit cache mount を活用**:
  `--mount=type=cache,target=/var/cache/apt,sharing=locked` で apt のダウン
  ロードキャッシュを保持。
- **multi-stage で「dev」「ci」を分ける**: 人間用 (zsh / sudo 等) を `dev`
  stage、CI 用 minimal を `base` stage に。同じ Dockerfile の中に同居させて
  `target:` で切り替える形 ([.devcontainer/Dockerfile](../.devcontainer/Dockerfile) 参照)。

## CMake

- **警告フラグは `target_compile_options(... PRIVATE ...)` で target スコープに**。
  `add_definitions(...)` のディレクトリスコープは避ける (フラグが意図しない
  別 target に漏れる)。
- **`-D` の universal フラグは [cmake/Options.cmake](../cmake/Options.cmake) の
  `CCBENCH_*` CACHE 変数に集約**。コマンドラインから上書き可能にしておく
  (例: `cmake -S . -B build -DCCBENCH_KEY_SIZE=16`)。
- **新しいプロトコルは `ccbench_add_protocol(...)` で宣言**
  ([cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake) 参照)。
  個別の `file(GLOB)` / `add_executable` / `add_definitions` 群は書かない。
- **`cmake_minimum_required(VERSION X.Y)` は迂闊に上げない** — devcontainer の
  cmake バージョンと CI の cmake バージョンの両方で評価する。

## GitHub Actions workflow

- **action は明示的なメジャー版を pin** (`actions/checkout@v6` のように)。
  `@main` や `@v6.1.2` のような floating / patch pin は避ける。
- **container job では `--user root` + git `safe.directory` 設定が必要**。
  詳細は [#46](https://github.com/thawk105/ccbench/pull/46) の経緯参照。
- **`paths-ignore` で無関係な変更で CI が動かないようにする** (docs/, README.md 等)。
- **secrets はリポジトリ secrets 経由でのみ参照**。ハードコードしない。

## C++

- **`tx.read` の戻り値は必ず check する** (CLAUDE.md の "tx.read returns
  Status — check it" セクション参照)。
- **未初期化のローカル変数は宣言時に default-init する** (`int x = 0;` /
  `T* p = nullptr;`)。GCC 13 の `-Wmaybe-uninitialized` が
  [#44](https://github.com/thawk105/ccbench/pull/44) で実バグを表面化した
  経緯あり。
- **`auto` で受けた `Status` を読み捨てない**。意図的に捨てたい場合は
  `(void) func(...)` + 理由のコメント、捨てたくない場合は明示的に check。

## Shell スクリプト

- **`set -euo pipefail`** を冒頭に置く (早期失敗 + 未定義変数の検出 + パイプ
  途中の失敗を見逃さない)。
- **long flag を優先** (`--no-install-recommends` / `--platform`)。ワンライナー
  でない限り、後で見たときに読みやすい方を選ぶ。

## Markdown / docs

- 全体的に **日本語と英語の混在 OK**。ただし 1 つのセクションでは統一する。
- **コードブロックには言語タグ** (` ```sh `, ` ```cmake `, ` ```cpp ` 等) を
  付けて GitHub の syntax highlight を効かせる。
- **相対リンクは repository root からの絶対パスではなく、文書からの相対パス**
  で書く ([../include/foo.hh](../include/) のように)。

## このドキュメント自体について

- **新しい規約に気付いたら、まず追記する** (PR レビューで毎回指摘するより
  ドキュメントに 1 回書いた方が長期コスト低い)。
- **規約の理由は短く併記する**: 「なぜそうするか」が分からないと、規約が形骸化
  して破られる。
- **規約を曲げるべき例外を見つけたら、その例外もこのドキュメントに書く**。
