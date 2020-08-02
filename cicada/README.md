# Cicada
It was proposed at SIGMOD'2017 by Hyeontaek Lim.

## How to use
- Build masstree
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used below building.
- Build 
```
$ make
```
- Confirm usage 
```
$ ./cicada.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./cicada.exe -tuple_num=1000 -max_ope=10 -thread_num=224 -rratio=100 -rmw=0 -zipf_skew=0 -ycsb=1 -p_wal=0 -s_wal=0 -clocks_per_us=2100 -io_time_ns=5 -group_commit_timeout_us=2 -group_commit=0 -gc_inter_us=10 -pre_reserve_version=10000 -worker1_insert_delay_rphase_us=0 -extime=3
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `INLINE_VERSION_OPT` : If this is 1, it use inline version optimization.
- `INLINE_VERSION_PROMOTION` : If this is 1, it use inline version promotion optimization.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `PARTITION_TABLE` : If this is 1, it devide the table into the number of worker threads not to occur read/write conflicts.
- `REUSE_VERSION` : If this is 1, it use special version cache optimization.
- `SINGLE_EXEC` : If this is 1, it behaves as single version concurrency control.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.
- `WRITE_LATEST_ONLY` : If this is 1, it restricts to use write latest only rules for write operation.
- `WORKER1_INSERT_DELAY_RPHASE` : If this is 1, worker 1 inserts delay at read phase for the time set at runtime arguments.

## Optimizations
- Backoff.
- Early aborts.
- Inline version.
- Inline version promotion.
- Rapid garbage collection.
- Reuse version.
- Sort write set by contention.
- Validation in **read phase** (for early aborts).
- Validation before inserting pending versions.
- Write latst only.
- Worker 1 inserts delay.

## Bug fix from original implementation.
- fix consistency checks.
