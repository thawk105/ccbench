# TicToc
It was proposed at SIGMOD'2016 by Xiangyao Yu.

## How to use
- Build 
```
$ make
```
- Confirm usage 
```
$ ./tictoc.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./tictoc.exe -clocks_per_us=2100 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `NO_WAIT_LOCKING_IN_VALIDATION` : If this is 1, it aborts immediately at detecting w-w conflicts in validation phase.
- `PREEMPTIVE_ABORTS` : It is early aborts.
- `SLEEP_READ_PHASE` : If this is set, it inserts delay for set value [clocks] in read phase.
- `TIMESTAMP_HISTORY` : It is multi-version of write timestamp.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.

## Optimizations
- Backoff.
- Early aborts.
- No-wait locking in validation phase.
- Reduce roop in concurrency control protocol.
- Timestamp history.

## Bug fix from original implementation.
- fix about condition of read timestamp update.
