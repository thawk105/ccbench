#test_cache_ana.sh(ermia)
tuple=1000000
maxope=10
thread=24
cpu_mhz=2400
extime=3
epoch=5

rratio=0
result=result_ermia_r0-10_cache-references.dat
result2=result_ermia_r0-10_cache-misses.dat
rm $result
rm $result2
echo "#rratio, cache-references, min, max" >> $result
echo "#rratio, cache-misses, min, max" >> $result2
echo "#./ermia.exe $tuple $maxope $thread $rratio $cpu_mhz $extime" >> $result
echo "#./ermia.exe $tuple $maxope $thread $rratio $cpu_mhz $extime" >> $result2

for ((rratio=0; rratio <= 10; ++rratio))
do
  echo "./ermia.exe $tuple $maxope $thread $rratio $cpu_mhz $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e cache-references,cache-misses -o ermia_cache_ana.txt ./ermia.exe $tuple $maxope $thread $rratio $cpu_mhz $extime
      tmp=`grep cache-references ./ermia_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep cache-misses ./ermia_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum: $sum,   tmp: $tmp"
      echo "sum2: $sum2,   tmp2: $tmp2"
  
    if test $i -eq 1 ; then
      max=$tmp
      min=$tmp
      max2=$tmp2
      min2=$tmp2
    fi
  
    flag=`echo "$tmp > $max" | bc -l`
    if test $flag -eq 1 ; then
      max=$tmp
    fi
  
    flag=`echo "$tmp < $min" | bc -l`
    if test $flag -eq 1 ; then
      min=$tmp
    fi
    flag=`echo "$tmp2 > $max2" | bc -l`
    if test $flag -eq 1 ; then
      max2=$tmp2
    fi
  
    flag=`echo "$tmp2 < $min2" | bc -l`
    if test $flag -eq 1 ; then
      min2=$tmp2
    fi
  done
  
  avg=`echo "$sum / $epoch" | bc -l`
  avg2=`echo "$sum2 / $epoch" | bc -l`
  echo "sum: $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg $avg"
  echo "avg2 $avg2"
  echo "max: $max"
  echo "max2: $max2"
  echo "min: $min"
  echo "min2: $min2"
  echo "$rratio $avg $min $max" >> $result
  echo "$rratio $avg2 $min2 $max2" >> $result2
done
