# ビルド

## ホスト要件

CCBench は x86_64 Linux (Debian/Ubuntu) を対象とする。x86 intrinsics (`__cpuid_count` in [include/cpu.hh](../include/cpu.hh)) と Linux 専用 API (`sched_setaffinity`, `SYS_gettid`, `<linux/fs.h>` in [include/fileio.hh](../include/fileio.hh)) を使うので、macOS など他のプラットフォームでネイティブにはビルドできない。macOS で開発する場合は [devcontainer](#devcontainer-macos--ubuntu-以外のホスト向け) を使う。

CI は GitHub Actions の `ubuntu-latest` で走る — [.github/workflows/build.yml](../.github/workflows/build.yml) 参照。push (どのブランチでも) と PR でトリガーされ、ccache と apt はキャッシュされる。

ビルド依存をインストール (CI もこれを使っている):

```sh
sudo apt-get update
sudo apt-get install -y $(cat build_tools/ubuntu.deps)
```

## 全部ビルドする (トップレベル CMake)

トップレベル [CMakeLists.txt](../CMakeLists.txt) が全プロトコルサブディレクトリをドライブする。標準的な out-of-source ビルド:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

バイナリは `build/cc/<protocol>/` の下に `<workload>_<protocol>.exe` という名前で生成される。例: `build/cc/silo/tpcc_silo.exe`、`build/cc/cicada/ycsb_cicada.exe`、`build/cc/mocc/bomb_mocc.exe`。

1 つのバイナリだけビルドする:

```sh
cmake --build build --target tpcc_silo.exe
```

sanitizer 付きの debug build (デフォルト) なら `-DCMAKE_BUILD_TYPE=Debug` を使う。Sanitizer の toggle はトップレベル `CMakeLists.txt` にある (`ENABLE_SANITIZER`, `ENABLE_UB_SANITIZER`, `ENABLE_COVERAGE`)。

## プロトコル単位のビルド

各 `cc/<protocol>/CMakeLists.txt` は単一の宣言的 `ccbench_add_protocol(...)` 呼び出し — 関数定義は [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake)、汎用のビルド時 tunable (`CCBENCH_KEY_SIZE`, `CCBENCH_BACK_OFF` など) は [cmake/Options.cmake](../cmake/Options.cmake) を参照。これらは cmake コマンドラインから上書きできる。各プロトコルとそれがサポートするワークロードのリストは [protocols_ja.md](protocols_ja.md) を参照。

トップレベル CMake は `ccache` が PATH にあれば **ccache** を自動でコンパイラランチャに設定する。約 34 バイナリ間のコンパイル重複を排除する (full warm rebuild ≈ 3 秒 vs cold で 30+ 秒)。`-DCCBENCH_CCACHE=OFF` で無効化可能。

`include/` 部品のコスト測定用マイクロベンチ ([microbench/](../microbench/)) はデフォルトでビルドされない。`-DCCBENCH_BUILD_MICROBENCH=ON` でオプトイン有効化する。

## 開発のためのビルドモード

- **Debug+ASan** (トップレベル Debug ビルドのデフォルト) は correctness 系の作業に最適 — 過去に捕まえた TPC-C バグ (`get_and_update_*` の use-after-free、`cast_to<Order>` の assertion、gcRecord UAF) はすべて最初にこのモードで顕在化した。Release ビルドでは見えなかった。
- **Release** はベンチマーク数値専用。CI は sanitizer なしで Release をビルド (`-DENABLE_SANITIZER=OFF`) — 実行はしない、コンパイルが通ることだけ検証する。

## コンパイラバージョン

devcontainer (`ubuntu:24.04` ベース、[.devcontainer/Dockerfile](../.devcontainer/Dockerfile) 参照) と CI (`ubuntu-latest`) はどちらも **GCC 13** を載せているので、devcontainer でビルドできれば CI でもビルドできる。以前はそうではなかった — #44 で、旧 devcontainer の GCC 11 と CI の GCC 13 が `-Wmaybe-uninitialized` で食い違って 3 ラウンドの CI 往復を起こした経緯がある。それ以降、両者を揃えてある。

## Devcontainer (macOS / Ubuntu 以外のホスト向け)

リポジトリは `linux/amd64` に pin した devcontainer ([.devcontainer/](../.devcontainer/)) を同梱している:

1. VS Code でリポジトリを開いて **Dev Containers: Reopen in Container** を実行
2. コンテナ内でトップレベル `cmake -S . -B build && cmake --build build` を実行 (サードパーティ依存は CMake の FetchContent が configure 時に自動取得・ビルドする)

image は `ghcr.io/thawk105/ccbench-devcontainer:latest` に publish されていて、`.devcontainer/` を変更するたびに [.github/workflows/devcontainer-image.yml](../.github/workflows/devcontainer-image.yml) で rebuild される。

> **Apple Silicon ではこれは QEMU emulation で動く。** 編集、コンパイル、correctness テストは OK。ただし **このマシンからベンチマーク数値を取らない** — 計測は実機の x86_64 Linux で行うこと。
