# microbench

特定の命令・操作の**コスト / 性能特性**を測るマイクロベンチ群。`include/`
の共通部品 (乱数・zipf・atomic ラッパ・rdtsc など) を**そのまま使って**、
それが「どれくらい速いか」を測る。部品の**正しさ**を検証するユニットテスト
(別イシュー) とは目的が直交する。

旧名は `instruction/` で独自 Makefile ベースだったが、用途が分かる名前へ
リネームし、トップレベル CMake にオプトインで接続した (#85)。

## ビルド

デフォルトビルドには含まれない (CI 時間を増やさないため)。`-DCCBENCH_BUILD_MICROBENCH=ON`
で有効化する:

```sh
cmake -S . -B build -DCCBENCH_BUILD_MICROBENCH=ON
cmake --build build
```

各バイナリは `build/microbench/<name>.exe` に出力される。

## ベンチ一覧

| バイナリ | 何を測るか |
|---|---|
| `rdtscBench.exe` | `rdtsc()` と `rdtscp()` の 1 回あたりの clock 数 |
| `pow_test.exe` | `std::pow(2, x)` の引数別の実行時間 |
| `xoroshiro.exe` | `Xoroshiro128Plus::next()` 1 回あたりの clock 数 |
| `fetch_add.exe` | N スレッドが共有 `std::atomic<uint64_t>` に `fetch_add` し続けたときのスループット (atomic RMW のコンテンション特性)。引数: `THREAD_NUM` |
| `zipf_dist_test.exe` | `FastZipf` の出力分布が skew どおりか (ヒストグラム出力)。引数: `LENGTH SKEW TRIAL` |
| `membench.exe` | メモリ帯域・レイテンシ。seq/rnd × read/write の 4 ワークロードを測る。引数: `THREAD_NUM` |

## legacy/

`legacy/` 配下は陳腐化 / ビルド不能で CMake 非対象。直す価値が出たら個別に
復活させる。

- `cache-test.cc` — CPU 番号ハードコード・本体コメントアウトで計測になっていない実験コード
- `mcslock_with_timeout.c` — `main` 無し・未定義シンボル前提のリファレンス写経片
- `masstree_simple_test/` — `.hpp` include など大補修が必要な Masstree 計測コード
