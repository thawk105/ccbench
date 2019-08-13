tuple=100000000
maxope=10
rratio=50
rmw=off
skew=0
ycsb=on
cpu_mhz=2100
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

result=result_silo-part_ycsbA_tuple100m_224th.dat
rm $result
echo "#tuple num, avg-tps, min-tps, max-tps, avg-ar, min-ar, max-ar, avg-camiss, min-camiss, max-camiss, avg-make_procedure_latency_rate, avg-read_latency_rate, avg-write_latency_rate, avg-vali_latency_rate avg-maxrss" >> $result
echo "#sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $epochtime $extime" >> $result

for ((gci=10; gci<=10; gci*=10))
do
  echo "sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $epochtime $extime"
  echo "Thread number $thread"
  
  sumTH=0
  sumAR=0
  sumCA=0
  sumML=0
  sumRL=0
  sumWL=0
  sumVL=0
  sumOL=0
  sumRE=0
  maxTH=0
  maxAR=0
  maxCA=0
  minTH=0 
  minAR=0
  minCA=0
  for ((i = 1; i <= epoch; ++i))
  do
    if test $host = $dbs11 ; then
      sudo perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $epochtime $extime > exp.txt
    fi
    if test $host = $chris41 ; then
      perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../silo.exe $tuple $maxope $thread $rratio $rmw $skew $ycsb $cpu_mhz $epochtime $extime > exp.txt
    fi
  
    tmpTH=`grep throughput ./exp.txt | awk '{print $2}'`
    tmpAR=`grep abort_rate ./exp.txt | awk '{print $2}'`
    tmpCA=`grep cache-misses ./ana.txt | awk '{print $4}'`
    tmpML=`grep make_procedure_latency_rate ./exp.txt | awk '{print $2}'`
    tmpRL=`grep read_latency_rate ./exp.txt | awk '{print $2}'`
    tmpWL=`grep write_latency_rate ./exp.txt | awk '{print $2}'`
    tmpVL=`grep vali_latency_rate ./exp.txt | awk '{print $2}'`
    tmpOL=`grep other_work_latency_rate ./exp.txt | awk '{print $2}'`
    tmpRE=`grep maxrss ./exp.txt | awk '{print $2}'`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
    sumAR=`echo "scale=4; $sumAR + $tmpAR" | bc | xargs printf %.4f`
    sumCA=`echo "$sumCA + $tmpCA" | bc`
    sumML=`echo "scale=4; $sumML + $tmpML" | bc | xargs printf %.4f`
    sumRL=`echo "scale=4; $sumRL + $tmpRL" | bc | xargs printf %.4f`
    sumWL=`echo "scale=4; $sumWL + $tmpWL" | bc | xargs printf %.4f`
    sumVL=`echo "scale=4; $sumVL + $tmpVL" | bc | xargs printf %.4f`
    sumOL=`echo "scale=4; $sumOL + $tmpOL" | bc | xargs printf %.4f`
    sumRE=`echo "$sumRE + $tmpRE" | bc`
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
  avgML=`echo "scale=4; $sumML / $epoch" | bc | xargs printf %.4f`
  avgRL=`echo "scale=4; $sumRL / $epoch" | bc | xargs printf %.4f`
  avgWL=`echo "scale=4; $sumWL / $epoch" | bc | xargs printf %.4f`
  avgVL=`echo "scale=4; $sumVL / $epoch" | bc | xargs printf %.4f`
  avgOL=`echo "scale=4; $sumOL / $epoch" | bc | xargs printf %.4f`
  avgRE=`echo "$sumRE / $epoch" | bc`
  echo "sumTH: $sumTH, sumAR: $sumAR, sumCA: $sumCA"
  echo "avgTH: $avgTH, avgAR: $avgAR, avgCA: $avgCA"
  echo "maxTH: $maxTH, maxAR: $maxAR, maxCA: $maxCA"
  echo "minTH: $minTH, minAR: $minAR, minCA: $minCA"
  echo ""
  echo "$gci $avgTH $minTH $maxTH $avgAR $minAR $maxAR $avgCA $minCA $maxCA $avgML $avgRL $avgWL $avgVL $avgOL $avgRE" >> $result
done
