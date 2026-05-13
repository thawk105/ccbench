# プロトコル

下記の各ディレクトリは [cc/](../cc/) 配下にあり、1 呼び出しで済む `CMakeLists.txt` を持つ (宣言的ヘルパーは [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake) 参照)。すべてトップレベル [CMakeLists.txt](../CMakeLists.txt) から駆動される。ビルド手順は [build_ja.md](build_ja.md)。

| Directory | プロトコル | YCSB | TPC-C | BoMB | 出典 |
|---|---|:-:|:-:|:-:|---|
| [cc/silo](../cc/silo/)     | Silo                                | ✓ | ✓ | ✓ | Tu et al., SOSP 2013 |
| [cc/cicada](../cc/cicada/) | Cicada                              | ✓ | ✓ | ✓ | Lim et al., SIGMOD 2017 |
| [cc/mocc](../cc/mocc/)     | MOCC                                | ✓ | ✓ | ✓ | Wang et al., VLDB 2017 |
| [cc/tictoc](../cc/tictoc/) | TicToc                              | ✓ | ✓ | ✓ | Yu et al., SIGMOD 2016 |
| [cc/ermia](../cc/ermia/)   | ERMIA (with SSN / latch-free SSN)   | ✓ | ✓ | ✓ | Kim et al., SIGMOD 2016; Wang et al., VLDB 2017 |
| [cc/oze](../cc/oze/)       | Oze                                 | ✓ | ✓ | ✓ | Multi-version OCC variant |
| [cc/si](../cc/si/)         | Snapshot Isolation                  | ✓ | ✓ | ✓ | ERMIA から SSN レイヤを剥がしたもの (anti-dependency チェック無し) |
| [cc/ss2pl](../cc/ss2pl/)   | Strong Strict 2-Phase Locking       | — | ✓ | ✓ | ベースラインのロック方式 |
| [cc/d2pl](../cc/d2pl/)     | Deterministic 2PL                   | — | — | ✓ | sBoMB と dBoMB のみサポート。pre-declared lock entries が必要なため TPC-C テンプレートには適用不能 |
| [cc/mvto](../cc/mvto/)     | Multi-Version Timestamp Ordering    | — | ✓ | ✓ | Reed, 1978 |

`cc/si` は `cc/ermia` から SSN レイヤを剥がして再実装したもの (元の `si/` は legacy uint64-key API で書かれていたためビルドから除外されている)。`cc/d2pl` は意図的に TPC-C 非対応 — その決定論的ロックモデルは `lock_entries_` の事前宣言を必要とするが、楽観的スタイルの TPC-C テンプレートはそれを提供しない。

## sBoMB / dBoMB サポート

各プロトコルの sBoMB (single-threaded BoMB) と dBoMB (deterministic BoMB) サポート状況は [build/PROTOCOL_MATRIX.md](../build/PROTOCOL_MATRIX.md) にあり、これは CMake configure 時に各 `ccbench_add_protocol(...)` 呼び出しの `WORKLOADS` 引数から **自動生成** される。現在のスナップショット:

| Protocol | YCSB | TPC-C | BoMB | sBoMB | dBoMB |
|---|:-:|:-:|:-:|:-:|:-:|
| `cc/silo/`   | ✓ | ✓ | ✓ | ✓ | — |
| `cc/mocc/`   | ✓ | ✓ | ✓ | ✓ | — |
| `cc/cicada/` | ✓ | ✓ | ✓ | ✓ | — |
| `cc/ermia/`  | ✓ | ✓ | ✓ | ✓ | — |
| `cc/tictoc/` | ✓ | ✓ | ✓ | ✓ | — |
| `cc/oze/`    | ✓ | ✓ | ✓ | — | — |
| `cc/si/`     | ✓ | ✓ | ✓ | ✓ | — |
| `cc/ss2pl/`  | — | ✓ | ✓ | — | — |
| `cc/mvto/`   | — | ✓ | ✓ | — | — |
| `cc/d2pl/`   | — | — | ✓ | ✓ | ✓ |

**この表を手で複製しないこと** — 最新のスナップショットが欲しければ cmake を再実行して `build/PROTOCOL_MATRIX.md` を確認する。

## プロトコルの追加 / チューニング

各プロトコルの CMakeLists.txt は単一の関数呼び出し:

```cmake
ccbench_add_protocol(<name>
  SOURCES   <shared .cc files>                # ワークロードのエントリポイントはここに入れない
  WORKLOADS ycsb tpcc bomb sbomb              # これらのタグの部分集合
  OPTIONS                                     # プロトコル固有の -D defines
    FOO=${CCBENCH_FOO}
    BAR                                       # bare flag = `-DBAR` (値なし)
)
```

汎用の `-D` フラグ (`KEY_SIZE`, `VAL_SIZE`, `BACK_OFF`, `ADD_ANALYSIS`, `MASSTREE_USE`) と `ccbench_common` + `ccbench::masstree` + `ccbench::mimalloc` への link は自動的に追加される。新しい cache オプションは [cmake/Options.cmake](../cmake/Options.cmake) に書く。値が空文字列の `OPTIONS` エントリは無視される — 旧式の `remove_definitions(-DFLAG)` と同じ意味。

新プロトコルのディレクトリを追加したら、トップレベル [CMakeLists.txt](../CMakeLists.txt) の `foreach(_proto …)` ループにも追加すること。

## デフォルトではビルドしないもの

| Directory | 備考 |
|---|---|
| [cc/occ](../cc/occ/) | K&R OCC (Kung & Robinson, 1981)。plain Makefile で書かれていて (legacy uint64-key API)、CMake ツリーに組み込まれていない。 |

有効にするには、`cc/occ/` を `ccbench_add_protocol(...)` エントリに port して、トップレベル [CMakeLists.txt](../CMakeLists.txt) の `foreach(_proto …)` ループに追加する。
