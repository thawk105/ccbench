#test_cache_ana.sh(silo)
maxope=10
thread=24
cpu_mhz=2400
epo40=40
extime=3
epoch=5

workload=0
result=result_silo_r10_L1-dcache-loads.dat
result2=result_silo_r10_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o silo_cache_ana.txt ./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime
      tmp=`grep L1-dcache-loads ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum : $sum,   tmp : $tmp"
      echo "sum2: $sum2,  tmp2: $tmp2"
  
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
  echo "sum : $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg  $avg"
  echo "avg2 $avg2"
  echo "max : $max"
  echo "min : $min"
  echo "max2: $max2"
  echo "min2: $min2"
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=1
result=result_silo_r8_L1-dcache-loads.dat
result2=result_silo_r8_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o silo_cache_ana.txt ./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime
      tmp=`grep L1-dcache-loads ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum : $sum,   tmp : $tmp"
      echo "sum2: $sum2,  tmp2: $tmp2"
  
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
  echo "sum : $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg  $avg"
  echo "avg2 $avg2"
  echo "max : $max"
  echo "min : $min"
  echo "max2: $max2"
  echo "min2: $min2"
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=2
result=result_silo_r5_L1-dcache-loads.dat
result2=result_silo_r5_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o silo_cache_ana.txt ./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime
      tmp=`grep L1-dcache-loads ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum : $sum,   tmp : $tmp"
      echo "sum2: $sum2,  tmp2: $tmp2"
  
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
  echo "sum : $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg  $avg"
  echo "avg2 $avg2"
  echo "max : $max"
  echo "min : $min"
  echo "max2: $max2"
  echo "min2: $min2"
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=3
result=result_silo_r2_L1-dcache-loads.dat
result2=result_silo_r2_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o silo_cache_ana.txt ./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime
      tmp=`grep L1-dcache-loads ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum : $sum,   tmp : $tmp"
      echo "sum2: $sum2,  tmp2: $tmp2"
  
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
  echo "sum : $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg  $avg"
  echo "avg2 $avg2"
  echo "max : $max"
  echo "min : $min"
  echo "max2: $max2"
  echo "min2: $min2"
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=4
result=result_silo_r0_L1-dcache-loads.dat
result2=result_silo_r0_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result
echo "#./silo.exe tuple $maxope $thread $workload $cpu_mhz $epo40 $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o silo_cache_ana.txt ./silo.exe $tuple $maxope $thread $workload $cpu_mhz $epo40 $extime
      tmp=`grep L1-dcache-loads ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./silo_cache_ana.txt | awk '{print $1}' | tr -d ,`
      sum=`echo "$sum + $tmp" | bc -l`
      sum2=`echo "$sum2 + $tmp2" | bc -l`
      echo "sum : $sum,   tmp : $tmp"
      echo "sum2: $sum2,  tmp2: $tmp2"
  
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
  echo "sum : $sum, epoch: $epoch"
  echo "sum2: $sum2, epoch: $epoch"
  echo "avg  $avg"
  echo "avg2 $avg2"
  echo "max : $max"
  echo "min : $min"
  echo "max2: $max2"
  echo "min2: $min2"
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

