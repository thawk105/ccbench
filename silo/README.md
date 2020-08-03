# Silo
It was proposed at SOSP'2013 by Stephen Tu.

## How to use
- Build masstree
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used by building silo.
- Build mimalloc
```
$ cd ../
$ ./bootstrap_mimalloc.sh
```
This makes ../third_party/mimalloc/out/release/libmimalloc.a used by building silo.
- Build 
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./silo.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./silo.exe -clocks_per_us=2100 -epoch_time=40 -extime=3 -max_ope=10 -rmw=0 -rratio=50 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
```

## How to customize options in CMakeLists.txt
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.<br>
default : `0`
- `BACK_OFF` : If this is 1, it use Cicada's backoff.<br>
default : `0`
- `KEY_SIZE` : The key size of key-value.<br>
default : `8`
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.<br>
default : `1`
- `NO_WAIT_LOCKING_IN_VALIDATION` : If this is 1, it aborts immediately at detecting w-w conflicts in validation phase. It derives this idea from TicToc.<br>
default : `1`
- `NO_WAIT_OF_TICTOC` : No-wait optimization of TicToc's optimizations.<br>
default : `0`
- `PARTITION_TABLE` : If this is 1, it devide the table into the number of worker threads not to occur read/write conflicts.<br>
default : `0`
- `PROCEDURE_SORT` : If this is 1, its transaction accesses records in ascending key order.<br>
default : `0`
- `SLEEP_READ_PHASE` : If this is set, it inserts delay for set value [clocks] in read phase.<br>
default : `0`
- `VAL_SIZE` : Value of key-value size. In other words, payload size.<br>
default : `4`
- `WAL` : If this is 1, it uses Write-Ahead Logging.<br>
default : `0`

## Custom build examples
```
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DKEY_SIZE=1000 -DVAL_SIZE=1000 ..
```
- Note: If you re-run cmake, don't forget to remove cmake cache.
```
$ rm CMakeCache.txt
```
The cmake cache definition data is used in preference to the command line definition data.

## CCBench's original optimizations
- Backoff.
- Extra read (SLEEP_READ_PHASE).
- No-wait in validation phase.
- No-wait of TicToc's optimizations.
- Reduce roop in concurrency control protocol.

## Workload optional parameters
- Partition
- Procedure sort

