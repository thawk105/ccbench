#ycsb-xrs.sh(silo)
tuple=10000000
maxope=16
rratioary=(50 95)
rmw=on
ycsb=on
cpumhz=2100
epochtime=40
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
    result=result_silo_ycsbA_tuple10m_ope16_rmw_skew0-099_th28.dat
  elif test $rratio = 95; then
    thread=28
    result=result_silo_ycsbB_tuple10m_ope16_rmw_skew0-099_th28.dat
  elif test $rratio = 100; then
    result=result_silo_ycsbC_tuple1k_skew0-099.dat
  else
    echo "BUG"
    exit 1
  fi
  rm $result

  echo "#tuple num, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss, avg-er, avg-rlr, avg-vlr" >> $result
  echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw skew $ycsb $cpumhz $epochtime $extime" >> $result
  ../silo.exe > exp.txt
  tmpStr=`grep ShowOptParameters ./exp.txt`
  echo "#$tmpStr" >> $result
  
  for ((tmpskew = 0; tmpskew <= 105; tmpskew += 10))
  do
    if test $tmpskew -eq 100 ; then
      tmpskew=95
    fi
    if test $tmpskew -eq 105 ; then
      tmpskew=99
    fi

    skew=`echo "scale=3; $tmpskew / 100.0" | bc -l | xargs printf %.2f`
  
    echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpumhz $epochtime $extime"
    
    sumTH=0
    sumAR=0
    sumCA=0
    maxTH=0
    maxAR=0
    maxCA=0
    minTH=0
    minAR=0
    minCA=0
    sumRLR=0
    sumVLR=0
    sumER=0
    for ((i = 1; i <= epoch; ++i))
    do
      if test $host = $dbs11 ; then
        sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpumhz $epochtime $extime > exp.txt
      fi
      if test $host = $chris41 ; then
        perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpumhz $epochtime $extime > exp.txt
      fi
    
      tmpTH=`grep throughput ./exp.txt | awk '{print $2}'`
      tmpAR=`grep abort_rate ./exp.txt | awk '{print $2}'`
      tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
      tmpER=`grep extra_reads ./exp.txt | awk '{print $2}'`
      tmpRLR=`grep read_latency_rate ./exp.txt | awk '{print $2}'`
      tmpVLR=`grep vali_latency_rate ./exp.txt | awk '{print $2}'`
      sumTH=`echo "$sumTH + $tmpTH" | bc`
      sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
      sumCA=`echo "$sumCA + $tmpCA" | bc`
      sumER=`echo "$sumER + $tmpER" | bc`
      sumRLR=`echo "scale=4; $sumRLR + $tmpRLR" | bc | xargs printf %.4f`
      sumVLR=`echo "scale=4; $sumVLR + $tmpVLR" | bc | xargs printf %.4f`
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
    avgER=`echo "$sumER / $epoch" | bc`
    avgRLR=`echo "scale=4; $sumRLR / $epoch" | bc | xargs printf %.4f`
    avgVLR=`echo "scale=4; $sumVLR / $epoch" | bc | xargs printf %.4f`
    echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
    echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA, avgER: $avgER, avgRLR: $avgRLR, avgVLR: $avgVLR"
    echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
    echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
    echo ""
    echo "$skew $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA $avgER $avgRLR $avgVLR" >> $result
  done
done
