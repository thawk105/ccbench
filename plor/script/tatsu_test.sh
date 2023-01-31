cmake -G Ninja -DCMAKE_BUILD_PE=Release -DVAL_SIZE=4 -DMASSTREE_USE=1 -DBACK_OFF=0 ..
ninja
for i in 56 112 168 224
do 
    numactl --interleave=all ./ss2pl.exe -clocks_per_us=2100 -extime=3 -max_ope=10 -rmw=0 -rratio=80 -thread_num=$i -tuple_num=1000000 -ycsb=1 -zipf_skew=0
done