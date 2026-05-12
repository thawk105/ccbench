# ERMIA
It was proposed at SIGMOD'2016 by Kangnyeon Kim.
SSN is serialization certifier which has to be executed serial.
Latch-free SSN was proposed at VLDB'2017 by Tianzheng Wang.

## How to use
- Build masstree
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used by building ermia.
- Build mimalloc
```
$ cd ../
$ ./bootstrap_mimalloc.sh
```
This makes ../third_party/mimalloc/out/release/libmimalloc.a used by building ermia.
- Build 
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./ermia.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./ermia.exe -tuple_num=1000 -max_ope=10 -thread_num=224 -rratio=100 -rmw=0 -zipf_skew=0 -ycsb=1 -clocks_per_us=2100 -gc_inter_us=10 -pre_reserve_version=10000 -pre_reserve_tmt_element=100 -extime=3
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
- Early aborts.
- Latch-free SSN.
- Leveraging existing infrastructure.
- Rapid garbage collection.
- Reduce cache line contention about transaction mapping table.
- Reuse version.
