# Bamboo

## Publicized Experiments
In build directory, execute:
```
$ ../script/low_contention.sh
$ ../script/high_contentionA.sh
$ ../script/high_contentionB.sh
```

## How to use
- Build masstree (optional)
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used 
- Build mimalloc
```
$ cd ../
$ ./bootstrap_mimalloc.sh
```
This makes ../third_party/mimalloc/out/release/libmimalloc.a used 
- Build 
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./plor.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./plor.exe -clocks_per_us=2100 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
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


## Implementation
- Lock : reader/writer lock
