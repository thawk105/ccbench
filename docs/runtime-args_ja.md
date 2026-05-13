# 実行時引数

CCBench のバイナリは [gflags](https://github.com/gflags/gflags) と glog を使うので、すべてのフラグにデフォルト値があり、任意の部分集合をコマンドラインから上書きできる。あるバイナリの全フラグリストは常に以下で得られる:

```sh
./build/cc/silo/tpcc_silo.exe -help
```

バイナリ名は `<workload>_<protocol>.exe` の形式で、`build/cc/<protocol>/` 配下に置かれる。各バイナリは以下の **共通フラグ** + ワークロード固有のフラグを受け付ける。

## 共通フラグ

全ワークロードバイナリで共通。

| Flag | 意味 |
|---|---|
| `-thread_num` | worker thread の数。 |
| `-extime` | 実行時間 (秒)。 |
| `-epoch_time` | epoch 間隔 (ミリ秒、epoch ベースのプロトコル向け)。 |
| `-clocks_per_us` | キャリブレーション: ホスト CPU の TSC tick / microsecond。 |

## YCSB フラグ (`ycsb_<protocol>.exe`)

| Flag | デフォルト | 意味 |
|---|---|---|
| `-ycsb_tuple_num` | `1000000` | レコード総数。 |
| `-ycsb_max_ope` | `10` | トランザクションごとの operation 数。 |
| `-ycsb_rratio` | `50` | 1 トランザクションの read 比率 (0〜100)。 |
| `-ycsb_rmw` | `false` | true なら、read の後に同じ key への write を伴う (read-modify-write)。 |
| `-ycsb_zipf_skew` | `0` | `[0, 1)` の範囲の Zipf skew。`0` は uniform。 |

## TPC-C フラグ (`tpcc_<protocol>.exe`)

TPC-C のトランザクション混合比 (%) の合計は 100 にする。下記 4 フラグの合計を 100 から引いた値が New-Order の比率。

| Flag | デフォルト | 意味 |
|---|---|---|
| `-tpcc_num_wh` | `1` | warehouse の数 (scale factor)。 |
| `-tpcc_perc_payment` | `43` | Payment トランザクションの %。 |
| `-tpcc_perc_order_status` | `4` | Order-Status トランザクションの %。 |
| `-tpcc_perc_delivery` | `4` | Delivery トランザクションの %。 |
| `-tpcc_perc_stock_level` | `4` | Stock-Level トランザクションの %。 |
| `-tpcc_interactive_ms` | `0` | SQL-equivalent unit ごとの sleep ミリ秒 (think time)。 |

## BoMB フラグ (`bomb_<protocol>.exe`, `sbomb_<protocol>.exe`, `dbomb_d2pl.exe`)

BoMB には多くの knob がある。下記はよく使うものだけ。ワークロード自体の説明は [README.md § BoMB](../README.md#bomb)、テーブルサイジング系の `-bomb_factory_size`、`-bomb_product_size`、`-bomb_work_size` などを含む完全なリストは `-help` を参照。

| Flag | デフォルト | 意味 |
|---|---|---|
| `-bomb_mixed_mode` | `false` | long + short の mixed ワークロードを有効化。 |
| `-bomb_mixed_short_rate` | `500` | short トランザクションの request rate。 |
| `-bomb_l1_thread_num` | `1` | L1 (long) トランザクションを走らせる thread 数。 |
| `-bomb_s1_thread_num` … `-bomb_s5_thread_num` | varies | S1〜S5 各 short トランザクションの thread 数。 |
| `-bomb_perc_s1` … `-bomb_perc_s4` | varies | short トランザクション間の % 混合比。 |
| `-bomb_use_cache` | `false` | キャッシュ済みの BoM tree を使う (静的設定)。 |
| `-bomb_rate_control` | `false` | rate-limited な request injection を有効化。 |

## 例

8 warehouse で Silo の TPC-C:

```sh
./build/cc/silo/tpcc_silo.exe -thread_num=8 -tpcc_num_wh=8 -extime=10
```

YCSB-A (50/50 r/w) を Cicada で:

```sh
./build/cc/cicada/ycsb_cicada.exe -thread_num=8 -ycsb_rratio=50 -ycsb_zipf_skew=0.9
```

BoMB mixed mode を Silo で:

```sh
./build/cc/silo/bomb_silo.exe -thread_num=8 -bomb_mixed_mode -bomb_mixed_short_rate=1000
```

各プロトコルの `README.md` にはプロトコル固有のフラグ (WAL、GC、version pre-reservation など) が追加で書かれている場合がある。
