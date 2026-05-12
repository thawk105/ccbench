#test_cache_ana.sh(cicada)
maxope=10
thread=24
wal=OFF
group_commit=OFF
cpu_mhz=2400
io_time_ns=5
group_commit_timeout_us=2
lock_release=E
extime=3
epoch=5

workload=0
result=result_cicada_r10_L1-dcache-loads.dat
result2=result_cicada_r10_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
      tmp=`grep L1-dcache-loads ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
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
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=1
result=result_cicada_r8_L1-dcache-loads.dat
result2=result_cicada_r8_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
      tmp=`grep L1-dcache-loads ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
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
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=2
result=result_cicada_r5_L1-dcache-loads.dat
result2=result_cicada_r5_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
      tmp=`grep L1-dcache-loads ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
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
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=3
result=result_cicada_r2_L1-dcache-loads.dat
result2=result_cicada_r2_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
      tmp=`grep L1-dcache-loads ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
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
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

workload=4
result=result_cicada_r0_L1-dcache-loads.dat
result2=result_cicada_r0_L1-dcache-load-misses.dat
rm $result
rm $result2
echo "#tuple, L1-dcache-loads, min, max" >> $result
echo "#tuple, L1-dcache-load-misses, min, max" >> $result2
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result2

for ((tuple=1000; tuple<=100000000; tuple=$tuple * 10))
do
  echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  sum=0
  max=0
  min=0 
  sum2=0
  max2=0
  min2=0  
  for ((i = 1; i <= epoch; ++i))
  do
      perf stat -e L1-dcache-loads,L1-dcache-load-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
      tmp=`grep L1-dcache-loads ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
      tmp2=`grep L1-dcache-load-misses ./cicada_cache_ana.txt | awk '{print $1}' | tr -d ,`
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
  echo "$tuple $avg $min $max" >> $result
  echo "$tuple $avg2 $min2 $max2" >> $result2
done

