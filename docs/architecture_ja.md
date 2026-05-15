# アーキテクチャ

リポジトリの全体像と、各プロトコルが従う内部規約をまとめる。ビルド手順は [build_ja.md](build_ja.md)、プロトコル × ワークロードの対応表は [protocols_ja.md](protocols_ja.md)、ワークロード仕様は [workloads_ja.md](workloads_ja.md) を参照。

## このリポジトリは何か

CCBench は主要な in-memory 並行性制御プロトコルを **共通の基盤の上に再実装** することで、同一のワークロードで比較できるようにしたもの (Tanabe et al., VLDB 2020)。現在は **TPC-C** と **BoMB** のワークロード、および追加のいくつかのプロトコル ([@jnmt](https://github.com/jnmt) が `vldb-paper` ブランチで追加したもの) も同梱している。

3 つのワークロード:

- **TPC-C** — 標準的な OLTP ベンチマーク
- **YCSB** — key-value ワークロード (read/write 比、Zipfian 偏り、など)
- **BoMB** — Bill-of-Materials Benchmark (長時間走る製品コスト計算 + 短い OLTP txn)

ワークロード仕様は [workloads_ja.md](workloads_ja.md) を参照。

## リポジトリレイアウト

- [include/](../include/) — 共通ヘッダ (atomics, rwlock, zipf, masstree wrapper など)
- [include/tpcc/](../include/tpcc/) — 統一された TPC-C フレームワーク (テーブル / クエリ / 5 トランザクション)
- [include/ycsb.hh](../include/ycsb.hh), [include/bomb.hh](../include/bomb.hh), [include/bomb_pessimistic.hh](../include/bomb_pessimistic.hh), [include/dbomb_deterministic.hh](../include/dbomb_deterministic.hh), [include/sbomb_deterministic.hh](../include/sbomb_deterministic.hh), [include/workload.hh](../include/workload.hh) — 各ワークロードのエントリポイント
- [common/](../common/) — 全プロトコル共通のソース
- [cc/](../cc/) — 各 CC プロトコルのサブディレクトリ (`cc/silo/`, `cc/cicada/`, ...)。各々 1 行で済む `CMakeLists.txt` と `<workload>_<protocol>.cc` のエントリポイントを持つ
- [third_party/](../third_party/) — submodules: `masstree`, `mimalloc`, `googletest`
- [build_tools/](../build_tools/) — bootstrap スクリプトと `ubuntu.deps` (apt パッケージリスト)
- [cmake/](../cmake/) — 共有 CMake モジュール: `CompileOptions.cmake`、[Options.cmake](../cmake/Options.cmake) (汎用の `-D` フラグ群)、[ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake) (`ccbench_add_protocol`)
- [docs/](.) — このディレクトリ
- [instruction/](../instruction/) — 個別 instruction のマイクロベンチマーク (cache, fetch_add, など)

## 共通の Transaction API (コンパイル時に強制)

ビルドされる全プロトコルは、[include/tpcc.hh](../include/tpcc.hh)、[include/bomb.hh](../include/bomb.hh)、[include/ycsb.hh](../include/ycsb.hh) のワークロードテンプレートが期待する `TxExecutor` API を実装する:

```cpp
Status read(Storage s, std::string_view key, TupleBody** body);
Status update(Storage s, std::string_view key, TupleBody&& body);   // SQL UPDATE; 該当行が無ければ WARN_NOT_FOUND
Status insert(Storage s, std::string_view key, TupleBody&& body);   // SQL INSERT
Status delete_record(Storage s, std::string_view key);
Status scan(..., std::vector<TupleBody*>& result);
Status scan(..., std::vector<TupleBody*>& result, int64_t limit);   // limit 形式は TPC-C が使う、必須
bool   commit();
void   abort();
```

この契約は [include/tx_executor_concept.hh](../include/tx_executor_concept.hh) によって **コンパイル時に強制** される。各プロトコルの `transaction.hh` の末尾で `static_assert(TxExecutorLike<TxExecutor>);` を呼んでおり、メソッドの欠如やシグネチャ不一致があると **そのプロトコル自身のビルドが** named diagnostic 付きで失敗する。「ビルドはできたのに TPC-C delivery で `scan` の limit overload が無くて落ちる」のような状況にはもうならない。

このチェックには C++20 の `concept` ではなく `is_detected` 風の SFINAE ヘルパーを使っている。バンドルされている `masstree-beta` upstream が C++20 で削除された `std::allocator::construct/destroy` を呼んでいるため、ツリー全体が C++17 に pin されているのが理由。

`tx.read` の意味論 (Status 戻り値、ミス時の挙動) は非自明なので、コーディング規約として [coding-conventions_ja.md](coding-conventions_ja.md) の C++ 節に書いている。

## スレッドごとの GC と cross-thread Tuple 参照

`cc/ermia/` と `cc/si/` は、スレッドごとに 2 種類のキューを持つ:

- `gcq_for_version_`: 中のエントリは `rcdptr_` 経由で `Tuple*` を参照する
- `gcq_for_record_`: `Tuple*` を直接持つ

スレッド A が削除した `Tuple` は A 自身の `gcq_for_record_` にしか入らないが、スレッド B の `gcq_for_version_` がそれをまだ参照している可能性がある。`gcRecord` での UAF を防ぐため、両プロトコルは EBR (Epoch-Based Reclamation) 風のガードを使う:

- 各スレッドは、commit 時の `gcq_for_version_` push、および `gcVersion` の pop ループ完了後に、自分の `gcq_for_version_` 内の **最小 cstamp** を `MinQueuedCstamp[thid]` に publish する
- `gcRecord` は、削除 cstamp が **グローバル最小よりも厳密に小さい** Tuple のみ free する

**どちらかの GC キューの push / pop 箇所を変える場合、対応する publish 呼び出しも必ず一緒に変えること** — 詳細は [cc/si/garbage_collection.cc](../cc/si/garbage_collection.cc) と [cc/ermia/garbage_collection.cc](../cc/ermia/garbage_collection.cc) のコメントを参照。
