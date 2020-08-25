# Silo' for TPC-C bench

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
Note: If you re-run cmake, don't forget to remove cmake cache.
```
$ rm CMakeCache.txt
```
The cmake cache definition data is used in preference to the command line definition data.
- Confirm usage 
```
$ ./silo.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./silo.exe -extime 5 -num_wh 4 -thread_num 4
```

## What is this?
This directory contains the codes that execute a part of TPC-C benchmark (NewOrder and Payment) using CCBench.
Currently CCBench supports only Silo protocol.

## What is TPC-C?
TPC-C benchmark is an industrial standard.
It generates a workload that emulates a retail application.
TPC-C consists of five types of transactions.
They are New-Order, Payment, Order-Status, Delivery, and Stock-Level.

## Example of TPC-C Usage
TPC-C has been used for many studies as follows.
1. Silo [P1] uses all the five transactions on the Silo system [S1]. 
2. TicToc [P2] uses only the NewOrder and Payment transactions on the DBx1000 system [S2]. 
3. FOEDUS [P3] uses all the five transactions on the FOEDUS system [S3]. 
4. MOCC [P4] uses all the five transactions on the FOEDUS system [S3]. 
5. Cicada [P5] uses all the five transactions extending the DBx1000 system [S2]. 
6. Latch-free SSN [P6] uses all the five transactions on the ERMIA system [S4]. 
7. STOv2 [P7] uses all the five transactions on the STO system [S5]. 
8. ACC [P8] uses all the five transactions using the ACC system based on the Doppel system [P15] which is not open source.
9. AOCC [P9] uses only the NewOrder, Payment, and the original Reward transactions on the DBx1000 system [S2]. 
10. Transaction batching and reordering [P10] does not use any TPC-C transactions on the Cicada system [S6]. 
11. IC3 [P11] uses all the five transactions extending the Silo system [S1].
12. Abyss paper [P12] uses only the NewOrder and Payment transactions on the DBx1000 system [S2]. 
13. MVCC evaluation paper [P13] uses all the five transactions extending the Peloton system [S7].
14. Trireme paper [P14] does not use any TPC-C transactions on the Trireme system which is not open source.

## Transactions and concurrency control protocols supported by CCBench
TPC-C related development at CCBench started on August 1, 2020.
As of August 31, 2020, CCBench only supports New-Order and Payment for transactions and only Silo for concurrency control.
CCBench plans to support all 5 types of transactions and 7 concurrency control methods (Silo, TicToc, Cicada, MOCC, ERMIA, SI, 2PL) within the fiscal year 2020.

## Implementation overview
As of August 31, 2020, the client program was developed with reference to the DBx1000 system and Cicada system.
We thank the DBx1000 and Cicada team for publishing the code.
For range searching, Masstree published by Kohler [1]  is used.
We thank Kohler.

## Future work
CCBench plans to adopt a parallel tree different from [1] in the future.
Because there are bugs in [1].
We have identified and fixed two of them [2,3].

## Reference
[1] Masstree code. https://github.com/kohler/masstree-beta.
[2] Debug around string slice.hh. https://github.com/thawk105/masstree-beta/commit/
77ef355f868a6db4eac7b44669c508d8db053502.
[3] Debug Masstree about Cast Issue. https://github.com/thawk105/masstree-beta/commit/
d4bcf7711dc027818b1719a5a4c29aee547c58f6
