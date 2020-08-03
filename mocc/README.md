# MOCC
It was proposed at VLDB'2017 by Tianzheng Wang.

## How to use
- Build masstree
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used by building mocc.
- Build mimalloc
```
$ cd ../
$ ./bootstrap_mimalloc.sh
```
This makes ../third_party/mimalloc/out/release/libmimalloc.a used by building mocc.
- Build 
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./mocc.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./mocc.exe -clocks_per_us=2100 -epoch_time=40 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0 -per_xx_temp=4096
```

## How to customize options in CMakeLists.txt
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.<br>
default : `0`
- `BACK_OFF` : If this is 1, it use Cicada's backoff.<br>
default : `0`
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.<br>
default : `0`
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
default : `1`
- `TEMPERATURE_RESET_OPT` : If this is 1, it uses new temprature control protocol which reduces contentions and improves throughput much.<br>
default : `1`
- `VAL_SIZE` : Value of key-value size. In other words, payload size.<br>
default : `4`

## Custom build examples
```
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DKEY_SIZE=1000 -DVAL_SIZE=1000 ..
```
- Note: If you re-run cmake, don't forget to remove cmake cache.
```
$ rm CMakeCache.txt
```
The cmake cache definition data is used in preference to the command line definition data.

## Optimizations
- Backoff.
- Early aborts (by setting threshold of whether it executes try lock or wait lock).
- New temprature protocol reduces contentions and improves throughput much.

## Missing features
- MQL lock. It uses custom reader-writer lock instead of MQL lock because author's experimental environment has few NUMA architecture.
