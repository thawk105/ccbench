#ycsbA-xrs-cache.sh(cicada)
maxope=10
thread=24
rratio=50
skew=0
ycsb=ON
wal=OFF
group_commit=OFF
cpu_mhz=2400
io_time_ns=5
group_commit_timeout_us=2
lock_release=E
extime=1
epoch=1

result=result_cicada_ycsbA_tuple100-10m_cachemiss.dat
rm $result
echo "#tuple num, cache-misses, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $rratio $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=10000000; tuple*=10))
do
  sum=0
  echo "./cicada.exe $tuple $maxope $thread $rratio $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
  echo "$tuple $epoch"
 
  max=0
  min=0 
  for ((i = 1; i <= epoch; ++i))
  do
    perf stat -e cache-misses,cache-references -o cicada-cache-ana.txt ./cicada.exe $tuple $maxope $thread $rratio $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
    tmp=`grep cache-misses ./cicada-cache-ana.txt | awk '{print $4}'`
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
  echo "$tuple $avg $min $max" >> $result
done

