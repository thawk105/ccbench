cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DVAL_SIZE=1000 -DMASSTREE_USE=0 -DBACK_OFF=1 ..
ninja
for i in 120 96 64 32 16 8 1 #8 16 32 64 96 120
do 
    numactl --interleave=all ./bamboo.exe -clocks_per_us=2100 -extime=3 -max_ope=16 -rmw=0 -rratio=50 -thread_num=$i -tuple_num=100000000 -ycsb=1 -zipf_skew=0.9
done