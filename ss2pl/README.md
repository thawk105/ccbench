# 2PL

## How to use
- Build 
```
$ make
```
- Confirm usage 
```
$ ./ss2pl.exe
```
- Execution example 
```
$ numactl --interleave=all ./ss2pl.exe 200 10 24 50 off 0 on 2100 3
```

## How to select build options in Makefile
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0.
- `BACK_OFF` : If this is 1, it use Cicada's backoff.
- `KEY_SORT` : If this is 1, its transaction accesses records in ascending key order.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.
- `DLR0` : Dead lock resolution is timeout.
- `DLR1` : Dead lock resolution is no-wait.

## Optimizations
- Backoff.
- Timeout of dead lock resolution.
- No-wait of dead lock resolution.

## Implementation
- Lock : reader/writer lock
