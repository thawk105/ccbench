#ycsbA-xgcidbsize.sh(ermia)
maxope=10
thread=24
rratio=50
skew=0
ycsb=ON
cpu_mhz=2400
extime=3
epoch=5

result=result_ermia_ycsbA_tuple100-10m_gci1us-100ms_cache-miss.dat
rm $result
echo "#tuple num, gci, throughput, min, max" >> $result
for ((tuple=100; tuple<=10000000; tuple*=10))
do
  for ((gci=1; gci<=100000; gci*=10))
  do
    sum=0
    echo "./ermia.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $gci $extime"
    echo "$tuple $epoch"
   
    max=0
    min=0 
    for ((i = 1; i <= epoch; ++i))
    do
      perf stat -e cache-misses,cache-references -o ermia-cache-ana.txt ./ermia.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $gci $extime
      tmp=`grep cache-misses ./ermia-cache-ana.txt | awk '{print $4}'`
      sum=`echo "$sum + $tmp" | bc -l`
      echo "sum: $sum,   tmp: $tmp"
  
      if test $i -eq 1 ; then
        max=$tmp
        min=$tmp
      fi
  
      flag=`echo "$tmp > $max" | bc -l`
      if test $flag -eq 1 ; then
        max=$tmp
      fi
      flag=`echo "$tmp < $min" | bc -l`
      if test $flag -eq 1 ; then
        min=$tmp
      fi
    done
    
    avg=`echo "$sum / $epoch" | bc -l`
    echo "sum: $sum, epoch: $epoch"
    echo "avg $avg"
    echo "max: $max"
    echo "min: $min"
    echo "$tuple $gci $avg $min $max" >> $result
  done
  echo "" >> $result
done
