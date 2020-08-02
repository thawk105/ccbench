# MOCC
It was proposed at VLDB'2017 by Tianzheng Wang.

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
$ ./mocc.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./mocc.exe -clocks_per_us=2100 -epoch_time=40 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0 -per_xx_temp=4096
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `TEMPERATURE_RESET_OPT` : If this is 1, it uses new temprature control protocol which reduces contentions and improves throughput much.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.

## Optimizations
- Backoff.
- Early aborts (by setting threshold of whether it executes try lock or wait lock).
- New temprature protocol reduces contentions and improves throughput much.

## Missing features
- MQL lock. It uses custom reader-writer lock instead of MQL lock because author's experimental environment has few NUMA architecture.
