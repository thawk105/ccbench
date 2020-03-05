#ycsb-xrs.sh(ss2pl)
tuple=10000000
maxope=16
rratioary=(50 95)
rmw=on
ycsb=on
cpu_mhz=2100
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

cd ../
make clean; make -j VAL_SIZE=100
cd script/

for rratio in "${rratioary[@]}"
do
  if test $rratio = 50; then
    thread=28
    result=result_2pl_ycsbA_tuple10m_ope16_rmw_skew0-099_th28.dat
  elif test $rratio = 95; then
    thread=28
    result=result_2pl_ycsbB_tuple10m_ope16_rmw_skew0-099_th28.dat
  elif test $rratio = 100; then
    result=result_ss2pl_ycsbC_tuple1k_skew0-099.dat
  else
    echo "BUG"
    exit 1
  fi
  rm $result

  echo "#Worker threads, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss" >> $result
  echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw skew $ycsb $cpu_mhz $extime" >> $result
  ../ss2pl.exe > exp.txt
  tmpStr=`grep ShowOptParameters ./exp.txt`
  echo "#$tmpStr" >> $result
  
  for ((tmpskew = 0; tmpskew <= 105; tmpskew += 10))
  do
    if test $tmpskew = 100 ; then
      tmpskew=95
    fi
    if test $tmpskew = 105 ; then
      tmpskew=99
    fi
    skew=`echo "scale=3; $tmpskew / 100.0" | bc -l | xargs printf %.2f`
  
    echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../ss2pl.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $extime" 
    
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
    
      tmpTH=`grep throughput ./exp.txt | awk '{print $2}'`
      tmpAR=`grep abort_rate ./exp.txt | awk '{print $2}'`
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
    echo "$skew $avgTH $minTH $maxTH $avgAR $minAR $maxAR, $avgCA $minCA $maxCA" >> $result
  done
done
