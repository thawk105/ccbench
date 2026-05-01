# Runtime arguments

CCBench binaries use [gflags](https://github.com/gflags/gflags) and glog, so
every flag has a default and any subset can be overridden on the command
line. The full list for a given binary is always available via:

```sh
./silo.exe -help
```

## Common YCSB-style flags

These are shared by most protocols. Defaults differ per protocol — check
`-help`.

| Flag | Meaning |
|---|---|
| `-thread_num` | Number of worker threads. |
| `-tuple_num` | Total number of records. |
| `-max_ope` | Operations per transaction. |
| `-rratio` | Read ratio of a single transaction (0–100). |
| `-rmw` | If set, reads are followed by a write to the same key (read-modify-write). |
| `-ycsb` | If set, run the YCSB workload. |
| `-zipf_skew` | Zipf skew in `[0, 1)`. `0` is uniform. |
| `-extime` | Execution time in seconds. |
| `-epoch_time` | Epoch interval in milliseconds (epoch-based protocols). |
| `-clocks_per_us` | Calibration: TSC ticks per microsecond on the host CPU. |

## Examples

Silo:

```sh
numactl --interleave=all ./silo.exe \
  -clocks_per_us=2100 -epoch_time=40 -extime=3 \
  -max_ope=10 -rmw=0 -rratio=50 -thread_num=224 \
  -tuple_num=1000000 -ycsb=1 -zipf_skew=0
```

Cicada (extra flags for WAL, GC, version pre-reservation):

```sh
numactl --interleave=all ./cicada.exe \
  -tuple_num=1000 -max_ope=10 -thread_num=224 \
  -rratio=100 -rmw=0 -zipf_skew=0 -ycsb=1 \
  -p_wal=0 -s_wal=0 -clocks_per_us=2100 \
  -io_time_ns=5 -group_commit_timeout_us=2 -group_commit=0 \
  -gc_inter_us=10 -pre_reserve_version=10000 \
  -worker1_insert_delay_rphase_us=0 -extime=3
```

Each protocol's `README.md` has its own canonical example.
