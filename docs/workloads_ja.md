# ワークロード

CCBench は 3 つのワークロードを提供する。それぞれプロトコルごとに `<workload>_<protocol>.exe` として `build/cc/<protocol>/` 配下にビルドされる。各バイナリが受け付けるフラグは [runtime-args_ja.md](runtime-args_ja.md) を参照。

- [BoMB](#bomb) — Bill of Materials Benchmark (長時間 + 短時間 OLTP の混合)
- [TPC-C](#tpc-c) — 標準的な OLTP ベンチマーク
- [YCSB](#ycsb) — key-value ワークロード

## BoMB

### 概要

BoM (Bill of Materials、ある製品を作るために必要な部材とコストの階層リスト) を使った product costing は、製造業のデータベースにおける主要な作業の一つ。従来はこれを OLTP システムがオフラインのとき、もしくは古い materialized view を使ってオンラインで行ってきた。しかしサプライチェーン混乱による原材料コストの頻繁な変動などにより、**オンデマンドな costing** のニーズが出てきた。サプライチェーン最適化のためには正確でリアルタイムなコスト計算が重要になる。

BoMB (BoM Benchmark) は、実在のパン製造会社 [Andersen](https://www.andersen-group.jp/english/) をベースに、こうしたオンデマンド costing をエミュレートする。各種業界に応用できるよう共通の構成要素とパラメータを抽出してある。ベンチマークは 7 つのテーブル上で、BoM を使った product costing を行う 1 つの長時間トランザクション (L1) と、5 つの短時間トランザクション (S1〜S5) から成る。

### テーブル

BoMB は次の 7 つのテーブルを使う。下線は primary key。`INT16`、`INT32`、`INT64` はそれぞれ 16/32/64 bit 整数。テーブルの cardinality (件数) は調整可能な [パラメータ](#パラメータ) で決まる。

| Name                | Attributes                                                                                                                                                              | 説明                                                                                                                                                                                                                                                                                                                                                                                                                                  | デフォルト件数  |
|:--------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:----------------|
| **factory**         | <ul><li><u>id</u> `INT32`</li> <li>name `VARCHAR`</li></ul>                                                                                                             | 企業の工場一覧を管理する。件数は `factories` パラメータで決まる。                                                                                                                                                                                                                                                                                                                                                                          | 8              |
| **item**            | <ul><li><u>id</u> `INT32`</li> <li>name `VARCHAR`, type `INT16`</li></ul>                                                                                               | 品目の名前と種別 (product / material / raw material) を管理する。件数は 3 つのパラメータ `product-types`、`material-types`、`raw-material-types` の和で決まる。                                                                                                                                                                                                                                                                            | 345,000        |
| **product**         | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>quantity `DOUBLE`</li></ul>                                                                  | 各工場で製造される製品とその数量を管理する。product costing では工場で現在製造中の製品を取得するために使われる。件数は `factories` × `target-products`。                                                                                                                                                                                                                                                                              | 800            |
| **bom**             | <ul><li><u>parent_item_id</u> `INT32`</li> <li><u>child_item_id</u> `INT32`</li> <li>quantity `DOUBLE`</li></ul>                                                        | 製品を製造するために必要な (中間 / 原) 材料と必要数量のリストを管理する。階層構造を持ち、論理的には BoM tree を表現する。件数の平均は `product-types` × `product-types` × `material-trees-per-product` × `material-tree-size` × `material-tree-size`/2 × `raw-materials-per-leaf`。                                                                                                                                                  | 54,000,000 (平均) |
| **material-cost**   | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>stock_quantity `DOUBLE`</li> <li>stock_amount `DOUBLE`</li></ul>                             | 工場ごと品目ごとの原材料コストを管理する。件数は `factories` × `raw-material-types`。                                                                                                                                                                                                                                                                                                                                                | 600,000        |
| **result-cost**     | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>result_cost `DOUBLE`</li></ul>                                                               | 工場 / 製品ごとの最新コスト計算結果。件数は `factories` × `target-products`。                                                                                                                                                                                                                                                                                                                                                            | 800            |
| **journal-voucher** | <ul><li><u>voucher_id</u> `INT64`</li> <li>date `DATE`</li> <li>debit `INT32`</li> <li>credit `INT32`</li> <li>amount `DOUBLE`</li> <li>description `VARCHAR`</li></ul> | result-cost テーブルの現在の product cost に基づくイベントが発生したときに振伝が挿入されるテーブル。ベンチマーク開始時は空。                                                                                                                                                                                                                                                                                                            | 0              |

### トランザクション

- **L1 (update-product-cost):** BoM tree を構築して product cost を計算する長時間トランザクション。まずランダムに 1 つの工場を選び、その工場で製造されている全製品とその数量を `product` テーブルから取得する。次に各製品の BoM tree を構築してコストを計算する。BoM tree 構築時は `bom` テーブルに加えて、S1 が更新する `material-cost` テーブルも参照する。結果を `result-cost` テーブルに書き込む (これは S2 が読む)。全製品についてこの処理を繰り返す。
- **S1 (update-material-cost):** 原材料のコストを変更する短時間トランザクション。工場と原材料をランダムに 1 つずつ選び、`material-cost` テーブルの該当 record を read-modify-write する。
- **S2 (issue-journal-voucher):** 計算済みの product cost に基づいて振伝を作る短時間トランザクション。工場をランダムに選び、`result-cost` テーブルをスキャンしてその工場の各製品のコストを取得する。次に、入力として与えられた数量とコストから金額を計算する。最後に、勘定科目と一緒に振伝レコードを `journal-voucher` テーブルに挿入する。挿入数は `target-products` パラメータに依存する。
- **S3 (change-product):** 古い製品を新規開発製品に置き換える短時間トランザクション。工場から製品をランダムに選び、削除する。新製品の一意な item ID を決め、`material-trees-per-product` に応じて root material をランダムに選び、選ばれた item ID で新しいレコードを `bom` テーブルに挿入する。
- **S4 (change-raw-material):** 製品の原材料を別のものに置き換える短時間トランザクション (例: 小麦 X を X' に変更)。`bom` テーブルからランダムにレコードを選び、`item` テーブルから原材料をランダムに選ぶ。`bom` / `item` テーブルのキャッシュ済みレコードを使う場合もある。旧レコードを削除し、選ばれた原材料で新レコードを挿入する。
- **S5 (change-product quantity):** ある工場の製品の製造数量を更新する短時間トランザクション (例: 需要計画の結果として)。工場と製品をランダムに 1 つずつ選び、`product` テーブルの該当レコードを与えられた数量で更新する。

### パラメータ

| Name                         | 説明                                              | デフォルト |
|:-----------------------------|:--------------------------------------------------|:-----------|
| `factories`                  | 工場の数。                                        | `8`        |
| `product-types`              | 製品種別の数。                                    | `72,000`   |
| `material-types`             | 中間材料種別の数。                                | `198,000`  |
| `raw-material-types`         | 原材料種別の数。                                  | `75,000`   |
| `material-trees-per-product` | 製品 1 つあたりの material tree の本数。          | `5`        |
| `material-tree-size`         | material tree 1 本あたりの material の数。        | `10`       |
| `raw-meterials-per-leaf`     | leaf material 1 つに含まれる原材料の数。          | `3`        |
| `target-products`            | 1 工場で製造される製品数。                        | `100`      |
| `target-materials`           | 更新対象とする原材料数。                          | `1`        |

## TPC-C

TPC-C は倉庫中心の注文処理アプリケーションをシミュレートする、OLTP システムのデファクト標準ベンチマーク。仕様の詳細は [公式ドキュメント](https://www.tpc.org/tpcc/) を参照。CCBench は TPC-C の 5 つのトランザクション (NewOrder, Payment, OrderStatus, Delivery, StockLevel) の full mix をサポートする。warehouse の数 (scale factor) と 5 つのトランザクションの混合比は、`-tpcc_*` フラグで指定できる ([runtime-args_ja.md § TPC-C flags](runtime-args_ja.md#tpc-c-flags-tpcc_protocolexe) 参照)。

## YCSB

[YCSB](https://github.com/brianfrankcooper/YCSB) は様々な "key-value" / "cloud" 系ストアのパフォーマンス評価のためのマイクロベンチマーク。トランザクショナル DB システムの評価にも使われる。CCBench の YCSB バイナリ (`ycsb_<protocol>.exe`) は以下のオプションを提供する:

- 1 トランザクションあたりの operation 数 (`-ycsb_max_ope`)
- トランザクションの read 比率 (`-ycsb_rratio`)
- 書き込みが read-modify-write か blind write か (`-ycsb_rmw`)
- テーブル内の総レコード数 (`-ycsb_tuple_num`)
- アクセスレコード選択時の Zipfian skew (`-ycsb_zipf_skew`)
