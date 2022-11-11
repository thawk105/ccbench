#ycsbB.sh(ss2pl)
maxope=10
rratio=95
rmw=off
skew=0
ycsb=on
cpu_mhz=2400
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

tuple=500
result=result_ss2pl_ycsbB_tuple500_masstree.dat
rm $result
echo "#Worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" >> $result
thread=1

echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
for ((i=1; i <= epoch; i++))
do
  if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi
  if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
echo ""
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
  for ((i=1; i <= epoch; i++))
  do
    if test $host = $dbs11 ; then
      sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
    if test $host = $chris41 ; then
      perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
  echo ""
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result
done

tuple=500000
result=result_ss2pl_ycsbB_tuple500k_masstree.dat
rm $result
echo "#Worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" >> $result
thread=1

echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
for ((i=1; i <= epoch; i++))
do
  if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi
  if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
echo ""
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
  for ((i=1; i <= epoch; i++))
  do
    if test $host = $dbs11 ; then
      sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
    if test $host = $chris41 ; then
      perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
  echo ""
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result
done

tuple=5000000
result=result_ss2pl_ycsbB_tuple5m_masstree.dat
rm $result
echo "#Worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" >> $result
thread=1

echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
for ((i=1; i <= epoch; i++))
do
  if test $host = $dbs11 ; then
    sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi
  if test $host = $chris41 ; then
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
  fi

  tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
  tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
  tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
  sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
echo ""
thout=`echo "$thread - 1" | bc`
echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
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
  for ((i=1; i <= epoch; i++))
  do
    if test $host = $dbs11 ; then
      sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
    if test $host = $chris41 ; then
      perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime > exp.txt
    fi
  
    tmpTH=`grep Throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
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
  echo ""
  thout=`echo "$thread - 1" | bc`
  echo "$thout $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result
done

