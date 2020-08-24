# Silo_ext for TPC-C bench
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
$ numactl --interleave=all ./silo.exe -extime 5 -num_wh 4 -thread_num 4
```

## Custom build examples
```
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
```
- Note: If you re-run cmake, don't forget to remove cmake cache.
```
$ rm CMakeCache.txt
```
The cmake cache definition data is used in preference to the command line definition data.
