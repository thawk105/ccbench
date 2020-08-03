# Snapshot Isolation
This implementation is used for analyzing base line of ERMIA(/SSN/latch-free SSN).<br>
So some parts is like ERMIA in implementation, configuration, and design to analyze concurrency controls deeply.<br>

## How to use
- Build masstree
```
$ cd ../
$ ./bootstrap.sh
```
This makes ../third_party/masstree/libkohler_masstree_json.a used by building si.
- Build mimalloc
```
$ cd ../
$ ./bootstrap_mimalloc.sh
```
This makes ../third_party/mimalloc/out/release/libmimalloc.a used by building si.
- Build 
```
$ mkdir build
$ cd build
$ cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
$ ninja
```
- Confirm usage 
```
$ ./si.exe
```
- Execution example 
```
$ numactl --interleave=all ./si.exe 1000 10 224 100 off 0 off 2100 10 100 0 3
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
- CFLAGS
 - Use either `-DCCTR_ON` or `-DCCTR_TW`. These meanings is described below in section **Details of Implementation**.

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
- Rapid garbage collection from Cicada's paper.
- Cicada's backoff (easy to use from ccbench/include/backoff.hh by a few restriction)
- Mitigate contentions for centralized counter. 

## Detail of Implementation
 This protocol follow two rules.
 1. Read operation reads the committed version which was latest at beginning of the transaction.
 2. First Updater (Committer) Wins Rule.
 
First Updater (Commiter) Wins Rule enforces that when two or more transactions execute a write operation on the same record, the lifetimes of those transactions do not overlap.

Normally this protocol gets a unique value from the shared counter at the start of the transaction and at the end of the transaction.
However, touching the shared counter twice in lifetime of transaction causes very high contention for the counter and degrading performance.

Author (tanabe) gives two selection which are able to be defined at Makefile.
 1. `-DCCTR_TW` means "Centralized counter is touched twice in the lifetime(begin/end) of transaction." It is normally technique.
 2. `-DCCTR_ON` means "Centralized counter is touched once in the lifetime of transaction." It is special technique. Counting up of shared counter only happen when a transaction commits. Instead of getting the count from the shared counter at the start of the transaction, get the latest commit timestamps of all the worker threads via the transaction mapping table. The element of the table may be refered by multiple concurrent worker thread. So getting and updating information are done by CAS. This technique reduces contentions for shared counter but increases overhead of memory management.

Author observed that author's technique `-DCCTR_ON` was better than `-DCCTR_TW` in some YCSB-A,C. Because the major bottleneck of performance generally is not memory management, but centralized counter. the author's technique is reducing cost about centralized counter and increasing memory management cost. Generally it contributes to improve performance. So normally it is better to set `-DCCTR_ON`.

