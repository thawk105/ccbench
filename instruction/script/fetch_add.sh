#membench.sh
thnr=1
epoch=3;

host=`hostname`
chris41="chris41.omni.hpcc.jp"
dbs11="dbs11"

#basically
inith=1
enth=24
inc=1
if test $host = $dbs11 ; then
  inith=1
  enth=224
  inc=28
fi

result=fetch_add.dat
rm $result
echo "#thnr, throughput, cache-miss-rate" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  if test $host = $dbs11 ; then
    if test $thread -eq 29 ; then
      thread=28
    fi
  fi
  sumtps=0
  sumcm=0
  for ((i = 1; i <= epoch; ++i))
  do
    echo "#thread: $thread, #epoch: $i"
    perf stat -e cache-misses,cache-references -o ana.txt numactl --interleave=all ../fetch_add $thread > out
    tmptps=`cat out`
    tmpcm=`grep cache-misses ./ana.txt | awk '{print $4}'`
    sumtps=`echo "$sumtps + $tmptps" | bc`
    sumcm=`echo "$sumcm + $tmpcm" | bc`
  done

  avgtps=`echo "$sumtps / $epoch" | bc`
  avgcm=`echo "$sumcm / $epoch" | bc`

  echo "avgtps: $avgtps, avgcm: $avgcm"
  echo "$thread $avgtps $avgcm" >> $result
done
