#test.sh(ermia)
cpu_mhz=2400
extime=3
epoch=3

result=fetch_add.dat
rm $result
echo "#worker threads, increments, min, max" >> $result
for ((thread=2; thread<=24; thread+=2))
do
  sum=0
  echo "./fetch_add.exe $thread $cpu_mhz $extime"

  max=0
  min=0
  for ((i=1; i <= epoch; ++i))
  do
    tmp=`./fetch_add.exe $thread $cpu_mhz $extime`
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
  avg=`echo "$sum / $epoch" | bc`
  echo "sum: $sum, epoch: $epoch"
  echo "avg: $avg"
  echo "max: $max"
  echo "min: $min"
  echo "$thread $avg $min $max" >> $result
done
