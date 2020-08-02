# 2PL

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
$ ./ss2pl.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./ss2pl.exe -clocks_per_us=2100 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.
- `DLR0` : Dead lock resolution is timeout.
- `DLR1` : Dead lock resolution is no-wait.

## Optimizations
- Backoff.
- Timeout of dead lock resolution.
- No-wait of dead lock resolution.

## Implementation
- Lock : reader/writer lock
