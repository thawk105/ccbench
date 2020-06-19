# OCC
The original proposal is as follows.

```
H. T. Kung and John T. Robinson. 1981. 
On optimistic methods for concurrency control. 
ACM Trans. Database Syst. 6, 2 (June 1981), 213-226. 
DOI=http://dx.doi.org/10.1145/319566.319567
```

## How to use
- Build 
```
$ make
```
- Confirm usage 
```
$ ./occ.exe -help
```
- Execution example 
```
$ numactl --interleave=all ./occ.exe -tuple_num=1000 -max_ope=10 -thread_num=224 -rratio=100 -rmw=0 -zipf_skew=0 -ycsb=1 -clocks_per_us=2100 -extime=3
```

## How to select build options in Makefile
- `OCC_GC_THRESHOLD` : Number of committed write sets to be kept before performing garbage collection.
- `ADD_ANALYSIS` : If this is 1, it is deeper analysis than setting 0 (currently no analysis point for OCC).
- `BACK_OFF` : If this is 1, it use backoff.
- `MASSTREE_USE` : If this is 1, it use masstree as data structure. If not, it use simple array αs data structure.
- `PARTITION_TABLE` : If this is 1, it devide the table into the number of worker threads not to occur read/write conflicts.
- `VAL_SIZE` : Value of key-value size. In other words, payload size.

## Optimizations
- Backoff.
