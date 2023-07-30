#tst200-1k-1m.sh(ermia)
maxope=10
rmw=off
skew=0
ycsb=off
cpu_mhz=2400
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

#kugiri
rratio=0
tuple=200
result=result_ermia_r0_tuple200.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=20
tuple=200
result=result_ermia_r2_tuple200.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=80
tuple=200
result=result_ermia_r8_tuple200.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=100
tuple=200
result=result_ermia_r10_tuple200.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=0
tuple=1000
result=result_ermia_r0_tuple1k.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=20
tuple=1000
result=result_ermia_r2_tuple1k.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=80
tuple=1000
result=result_ermia_r8_tuple1k.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=100
tuple=1000
result=result_ermia_r10_tuple1k.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=0
tuple=1000000
result=result_ermia_r0_tuple1m.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=20
tuple=1000000
result=result_ermia_r2_tuple1m.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=80
tuple=1000000
result=result_ermia_r8_tuple1m.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

#kugiri
rratio=100
tuple=1000000
result=result_ermia_r10_tuple1m.dat
rm $result
echo "#worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result
thread=2

echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
echo "Thread number $thread"

sumTH=0
sumAR=0
sumCA=0
maxTH=0
maxAR=0
maxCA=0
minTH=0
minAR=0
minCA=0
for ((i=1; i <= epoch; ++i))
do
  if test $host = $dbs11 ; then
  sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi
  if test $host = $chris41 ; then
  perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
  sumCA=`echo "$sumCA + $tmpCA" | bc`
  echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"

  if test $i -eq 1 ; then
    maxTH=$tmpTH
    maxAR=$tmpAR
    maxCA=$tmpCA
    minTH=$tmpTH
    minAR=$tmpAR
    minCA=$tmpCA
  fi

  flag=`echo "$tmpTH > $maxTH" | bc`
  if test $flag -eq 1 ; then
    maxTH=$tmpTH
  fi
  flag=`echo "$tmpAR > $maxAR" | bc`
  if test $flag -eq 1 ; then
    maxAR=$tmpAR
  fi
  flag=`echo "$tmpCA > $maxCA" | bc`
  if test $flag -eq 1 ; then
    maxCA=$tmpCA
  fi

  flag=`echo "$tmpTH < $minTH" | bc`
  if test $flag -eq 1 ; then
    minTH=$tmpTH
  fi
  flag=`echo "$tmpAR < $minAR" | bc`
  if test $flag -eq 1 ; then
    minAR=$tmpAR
  fi
  flag=`echo "$tmpCA < $minCA" | bc`
  if test $flag -eq 1 ; then
    minCA=$tmpCA
  fi 
done
avgTH=`echo "$sumTH / $epoch" | bc`
avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
avgCA=`echo "$sumCA / $epoch" | bc`
echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
echo ""

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0
  minAR=0
  minCA=0
  for ((i=1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
    if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep AbortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "$sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    echo "tmpTH: $tmpTH, tmpAR: $tmpAR, tmpCA: $tmpCA"
  
    if test $i -eq 1 ; then
      maxTH=$tmpTH
      maxAR=$tmpAR
      maxCA=$tmpCA
      minTH=$tmpTH
      minAR=$tmpAR
      minCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH > $maxTH" | bc`
    if test $flag -eq 1 ; then
      maxTH=$tmpTH
    fi
    flag=`echo "$tmpAR > $maxAR" | bc`
    if test $flag -eq 1 ; then
      maxAR=$tmpAR
    fi
    flag=`echo "$tmpCA > $maxCA" | bc`
    if test $flag -eq 1 ; then
      maxCA=$tmpCA
    fi
  
    flag=`echo "$tmpTH < $minTH" | bc`
    if test $flag -eq 1 ; then
      minTH=$tmpTH
    fi
    flag=`echo "$tmpAR < $minAR" | bc`
    if test $flag -eq 1 ; then
      minAR=$tmpAR
    fi
    flag=`echo "$tmpCA < $minCA" | bc`
    if test $flag -eq 1 ; then
      minCA=$tmpCA
    fi 
  done
  avgTH=`echo "$sumTH / $epoch" | bc`
  avgAR=`echo "scale=4; $sumAR / $epoch" | bc | xargs printf %.4f`
  avgCA=`echo "$sumCA / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
  echo ""
done

