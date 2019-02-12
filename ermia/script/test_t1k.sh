#test_t1k.sh(ermia)
tuple=1000
maxope=10
cpu_mhz=2400
extime=3
epoch=5

workload=0
result=result_ermia_r10_tuple1k_ar.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./ermia.exe $tuple $maxope thread $workload $cpu_mhz $extime" >> $result

thread=2
sum=0
echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

max=0
min=0
for ((i=1; i <= epoch; ++i))
do
  tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
  sum=`echo "$sum + $tmp" | bc -l`
  echo "sum: $sum,  tmp: $tmp"

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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
  sum=0
  echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
    sum=`echo "$sum + $tmp" | bc -l`
    echo "sum: $sum,  tmp: $tmp"

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
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avg $min $max" >> $result
done

workload=1
result=result_ermia_r8_tuple1k_ar.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./ermia.exe $tuple $maxope thread $workload $cpu_mhz $extime" >> $result

thread=2
sum=0
echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

max=0
min=0
for ((i=1; i <= epoch; ++i))
do
  tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
  sum=`echo "$sum + $tmp" | bc -l`
  echo "sum: $sum,  tmp: $tmp"

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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
  sum=0
  echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
    sum=`echo "$sum + $tmp" | bc -l`
    echo "sum: $sum,  tmp: $tmp"

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
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avg $min $max" >> $result
done

workload=2
result=result_ermia_r5_tuple1k_ar.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./ermia.exe $tuple $maxope thread $workload $cpu_mhz $extime" >> $result

thread=2
sum=0
echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

max=0
min=0
for ((i=1; i <= epoch; ++i))
do
  tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
  sum=`echo "$sum + $tmp" | bc -l`
  echo "sum: $sum,  tmp: $tmp"

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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
  sum=0
  echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
    sum=`echo "$sum + $tmp" | bc -l`
    echo "sum: $sum,  tmp: $tmp"

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
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avg $min $max" >> $result
done

workload=3
result=result_ermia_r2_tuple1k_ar.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./ermia.exe $tuple $maxope thread $workload $cpu_mhz $extime" >> $result

thread=2
sum=0
echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

max=0
min=0
for ((i=1; i <= epoch; ++i))
do
  tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
  sum=`echo "$sum + $tmp" | bc -l`
  echo "sum: $sum,  tmp: $tmp"

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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
  sum=0
  echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
    sum=`echo "$sum + $tmp" | bc -l`
    echo "sum: $sum,  tmp: $tmp"

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
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avg $min $max" >> $result
done

workload=4
result=result_ermia_r0_tuple1k_ar.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./ermia.exe $tuple $maxope thread $workload $cpu_mhz $extime" >> $result

thread=2
sum=0
echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

max=0
min=0
for ((i=1; i <= epoch; ++i))
do
  tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
  sum=`echo "$sum + $tmp" | bc -l`
  echo "sum: $sum,  tmp: $tmp"

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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
  sum=0
  echo "./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./ermia.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
    sum=`echo "$sum + $tmp" | bc -l`
    echo "sum: $sum,  tmp: $tmp"

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
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avg $min $max" >> $result
done

