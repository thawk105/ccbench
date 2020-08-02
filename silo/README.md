# Silo
It was proposed at SOSP'2013 by Stephen Tu.

## How to use
- Build masstree
```js
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
$ ./silo.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./silo.exe -clocks_per_us=2100 -epoch_time=40 -extime=3 -max_ope=10 -rmw=0 -rratio=50 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `NO_WAIT_LOCKING_IN_VALIDATION` : If this is 1, it aborts immediately at detecting w-w conflicts in validation phase. It derives this idea from TicToc.
- `PARTITION_TABLE` : If this is 1, it devide the table into the number of worker threads not to occur read/write conflicts.
- `PROCEDURE_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `SLEEP_READ_PHASE` : If this is set, it inserts delay for set value [clocks] in read phase.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.
- `WAL` : If this is 1, it uses Write-Ahead Logging.

## Optimizations
- Backoff.
- No-wait in validation phase.
- Reduce roop in concurrency control protocol.

## Workload optional parameters
- Partition
- Procedure sort

