# ERMIA
It was proposed at SIGMOD'2016 by Kangnyeon Kim.
SSN is serialization certifier which has to be executed serial.
Latch-free SSN was proposed at VLDB'2017 by Tianzheng Wang.

## How to use
- Build 
```
$ make
```
- Confirm usage 
```
$ ./ermia.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./ermia.exe -tuple_num=1000 -max_ope=10 -thread_num=224 -rratio=100 -rmw=0 -zipf_skew=0 -ycsb=1 -clocks_per_us=2100 -gc_inter_us=10 -pre_reserve_version=10000 -pre_reserve_tmt_element=100 -extime=3
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.

## Optimizations
- Backoff.
- Early aborts.
- Latch-free SSN.
- Leveraging existing infrastructure.
- Rapid garbage collection.
- Reduce cache line contention about transaction mapping table.
- Reuse version.
