# Runtime arguments

CCBench binaries use [gflags](https://github.com/gflags/gflags) and glog, so
every flag has a default and any subset can be overridden on the command
line. The full list for a given binary is always available via:

```sh
./build/cc/silo/tpcc_silo.exe -help
```

Binaries are named `<workload>_<protocol>.exe` and live under
`build/cc/<protocol>/`. Each binary accepts the **common** flags below plus
the flags for its specific workload.

## Common flags

These are shared across all workload binaries.

| Flag | Meaning |
|---|---|
| `-thread_num` | Number of worker threads. |
| `-extime` | Execution time in seconds. |
| `-epoch_time` | Epoch interval in milliseconds (epoch-based protocols). |
| `-clocks_per_us` | Calibration: TSC ticks per microsecond on the host CPU. |

## YCSB flags (`ycsb_<protocol>.exe`)

| Flag | Default | Meaning |
|---|---|---|
| `-ycsb_tuple_num` | `1000000` | Total number of records. |
| `-ycsb_max_ope` | `10` | Operations per transaction. |
| `-ycsb_rratio` | `50` | Read ratio of a single transaction (0–100). |
| `-ycsb_rmw` | `false` | If true, reads are followed by a write to the same key. |
| `-ycsb_zipf_skew` | `0` | Zipf skew in `[0, 1)`. `0` is uniform. |

## TPC-C flags (`tpcc_<protocol>.exe`)

The TPC-C transaction mix percentages must sum to 100; the remainder after
the four flags below is the New-Order percentage.

| Flag | Default | Meaning |
|---|---|---|
| `-tpcc_num_wh` | `1` | Number of warehouses (scale factor). |
| `-tpcc_perc_payment` | `43` | % of Payment transactions. |
| `-tpcc_perc_order_status` | `4` | % of Order-Status transactions. |
| `-tpcc_perc_delivery` | `4` | % of Delivery transactions. |
| `-tpcc_perc_stock_level` | `4` | % of Stock-Level transactions. |
| `-tpcc_interactive_ms` | `0` | Sleep ms per SQL-equivalent unit (think time). |

## BoMB flags (`bomb_<protocol>.exe`, `sbomb_<protocol>.exe`, `dbomb_d2pl.exe`)

BoMB has many knobs; the ones below are the most common. See
[README.md § BoMB](../README.md#bomb) for the workload description, and
`-help` for the full list including table sizing parameters
(`-bomb_factory_size`, `-bomb_product_size`, `-bomb_work_size`, etc.).

| Flag | Default | Meaning |
|---|---|---|
| `-bomb_mixed_mode` | `false` | Enable mixed long+short workload. |
| `-bomb_mixed_short_rate` | `500` | Request rate of short transactions. |
| `-bomb_l1_thread_num` | `1` | Threads running the L1 (long) transaction. |
| `-bomb_s1_thread_num` … `-bomb_s5_thread_num` | varies | Threads per S1–S5 short transaction. |
| `-bomb_perc_s1` … `-bomb_perc_s4` | varies | Percentage mix among short transactions. |
| `-bomb_use_cache` | `false` | Use cached BoM tree (static setting). |
| `-bomb_rate_control` | `false` | Enable rate-limited request injection. |

## SS2PL-specific flags (`*_ss2pl.exe`, only when built with `CCBENCH_SS2PL_DLR=0`)

The deadlock resolution strategy is chosen at configure time with
`-DCCBENCH_SS2PL_DLR=<0|1>`. The default is `1` (no-wait), which is the
historical behaviour. Only a binary built with `0` (timeout) accepts the flag
below.

| Flag | Default | Meaning |
|---|---|---|
| `-ss2pl_dlr0_timeout_us` | `1000` | Upper bound, in microseconds, on how long a single record lock acquisition waits. The transaction aborts when it expires. The bound is converted to TSC ticks with `-clocks_per_us`, so that value has to be correct. |

## Examples

TPC-C on Silo with 8 warehouses:

```sh
./build/cc/silo/tpcc_silo.exe -thread_num=8 -tpcc_num_wh=8 -extime=10
```

YCSB-A (50/50 r/w) on Cicada:

```sh
./build/cc/cicada/ycsb_cicada.exe -thread_num=8 -ycsb_rratio=50 -ycsb_zipf_skew=0.9
```

BoMB mixed mode on Silo:

```sh
./build/cc/silo/bomb_silo.exe -thread_num=8 -bomb_mixed_mode -bomb_mixed_short_rate=1000
```

Each protocol's `README.md` may have additional protocol-specific flags
(WAL, GC, version pre-reservation, etc.).
