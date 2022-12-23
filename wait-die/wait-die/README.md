# SS2PL wait-die variant

## How to use
- Build masstree in ccbench directory
```
$ ./build_tools/bootstrap.sh
```
This makes ccbench/third_party/masstree/libkohler_masstree_json.a used by building ss2pl.
- Build mimalloc
```
$ ./build_tools/bootstrap_mimalloc.sh
```
This makes ccbench/third_party/mimalloc/out/release/libmimalloc.a used by building ss2pl.
- Build in wound-wait directory
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./waitdie.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./waitdie.exe -clocks_per_us=2100 -extime=3 -max_ope=10 -rmw=0 -rratio=100 -thread_num=224 -tuple_num=1000000 -ycsb=1 -zipf_skew=0
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

## Optimizations
- Backoff.
- Timeout of dead lock resolution.
- No-wait of dead lock resolution.

## Implementation
- Lock : reader/writer lock
