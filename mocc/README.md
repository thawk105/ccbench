# MOCC
It was proposed at VLDB'2017 by Tianzheng Wang.

## How to use
- Build 
```
$ make
```
- Confirm usage 
```
$ ./mocc.exe
```
- Execution example 
```
$ numactl --interleave=all ./cicada.exe 1000 10 224 100 off 0 on off off 2100 5 2 10 10000 0 3
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `TEMPERATURE_RESET_OPT` : If this is 1, it uses new temprature control protocol which reduces contentions and improves throughput much.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.

## Optimizations
- Backoff.
- Early aborts (by setting threshold of whether it executes try lock or wait lock).
- New temprature protocol reduces contentions and improves throughput much.

