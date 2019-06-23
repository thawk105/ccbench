#ycsb-xrs.sh(ermia)
tuple=1000
maxope=10
rratio=50
rmw=off
skew=0
ycsb=on
cpu_mhz=2400
gci=10
extime=3
epoch=3


host=`hostname`
chris41="chris41.omni.hpcc.jp"
dbs11="dbs11"

#basically
thread=24
if  test $host = $dbs11 ; then
  thread=224
fi

result=result_ermia_ycsbA_tuple1k_skew0-099.dat
rm $result
echo "#tuple num, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
echo "#perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ermia.exe $tuple $maxope thread $rratio $rmw $skew $ycsb $cpu_mhz $gci $extime" >> $result

for ((tmpskew = 0; tmpskew <= 105; tmpskew += 10))
do
  if test $tmpskew = 100 ; then
    tmpskew=95
  fi
  if test $tmpskew = 105 ; then
    tmpskew=99
  fi
  skew=`echo "scale=3; $tmpskew / 100.0" | bc -l | xargs printf %.2f`

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
    tmpAR=`grep abortRate ./exp.txt | awk '{print $2}'`
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
  echo ""
  echo "$skew $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA" >> $result
done
