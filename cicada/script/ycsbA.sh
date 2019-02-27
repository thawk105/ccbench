#ycsb-a.sh(cicada)
maxope=10
rratio=50
rmw=off
skew=0
ycsb=on
wal=off
group_commit=off
cpu_mhz=2400
io_time_ns=5
group_commit_timeout_us=2
lock_release=E
gci=10
extime=3
epoch=3

host=`hostname`
chris41="chris41.omni.hpcc.jp"
dbs11="dbs11"

#basically
inith=4
enth=24
inc=4
if  test $host = $dbs11 ; then
inith=28
enth=224
inc=28
fi

tuple=10000
result=result_cicada_ycsbA_tuple10k_key50-val2k_numaia.dat
rm $result
echo "#worker thread, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar" >> $result
echo "#../cicada.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime" >> $result
thread=2

echo "../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
maxTH=0
maxAR=0
minTH=0 
minAR=0 
for ((i = 1; i <= epoch; ++i))
do
  numactl --interleave=all ../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime > exp.txt
  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    minTH=$tmpTH
    minAR=$tmpAR
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
echo "sumTH: $sumTH, sumAR: $sumAR"
echo "avgTH: $avgTH, avgAR: $avgAR"
echo "maxTH: $maxTH, maxAR: $maxAR"
echo "minTH: $minTH, minAR: $minAR"
echo ""
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime"
  echo "Thread number $thread"

  sumTH=0
  sumAR=0
  maxTH=0
  maxAR=0
  minTH=0 
  minAR=0 
  for ((i=1; i <= epoch; ++i))
  do
    numactl --interleave=all ../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime > exp.txt
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR"

    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      minTH=$tmpTH
      minAR=$tmpAR
    fi

    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi

    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
  done

  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  echo "sumTH: $sumTH, sumAR: $sumAR"
  echo "avgTH: $avgTH, avgAR: $avgAR"
  echo "maxTH: $maxTH, maxAR: $maxAR"
  echo "minTH: $minTH, minAR: $minAR"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR" >> $result
done

tuple=1000000
result=result_cicada_ycsbA_tuple1m_key50-val2k_numaia.dat
rm $result
echo "#worker thread, ave-tps, min-tps, max-tps, ave-ar, min-ar, max-ar" >> $result
echo "#../cicada.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime" >> $result
thread=2

echo "../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
maxTH=0
maxAR=0
minTH=0 
minAR=0 
for ((i = 1; i <= epoch; ++i))
do
  numactl --interleave=all ../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime > exp.txt
  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    minTH=$tmpTH
    minAR=$tmpAR
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
echo "sumTH: $sumTH, sumAR: $sumAR"
echo "avgTH: $avgTH, avgAR: $avgAR"
echo "maxTH: $maxTH, maxAR: $maxAR"
echo "minTH: $minTH, minAR: $minAR"
echo ""
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime"
  echo "Thread number $thread"

  sumTH=0
  sumAR=0
  maxTH=0
  maxAR=0
  minTH=0 
  minAR=0 
  for ((i=1; i <= epoch; ++i))
  do
    numactl --interleave=all ../cicada.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $gci $extime > exp.txt
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR"

    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      minTH=$tmpTH
      minAR=$tmpAR
    fi

    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi

    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
  done

  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  echo "sumTH: $sumTH, sumAR: $sumAR"
  echo "avgTH: $avgTH, avgAR: $avgAR"
  echo "maxTH: $maxTH, maxAR: $maxAR"
  echo "minTH: $minTH, minAR: $minAR"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR" >> $result
done

