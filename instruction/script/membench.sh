#membench.sh
thnr=1
alc=1;
epoch=3;

host=`hostname`
chris41="chris41.omni.hpcc.jp"
dbs11="dbs11"

#basically
inith=4
enth=24
inc=4
if test $host = $dbs11 ; then
  inith=28
  enth=224
  inc=28
fi

result=membench.dat
rm $result
echo "#array size was $alc GB." >> $result
echo "#thnr, srbw, swbw, rrbw, rwbw" >> $result

for ((thread=$inith; thread<=$enth; thread+=$inc))
do
  sumSRBW=0
  sumSWBW=0
  sumRRBW=0
  sumRWBW=0
  for ((i = 1; i <= epoch; ++i))
  do
    numactl --localalloc ../membench.exe $thread $alc > out
    echo "thread $thread"
    cat out | grep -v Th\#
    echo ""

    tmpSRBW=`grep -v Th# out | grep sequentialRead | awk '{print $2}'`
    tmpSWBW=`grep -v Th#  out | grep sequentialWrite | awk '{print $2}'`
    tmpRRBW=`grep -v Th#  out | grep randomRead | awk '{print $2}'`
    tmpRWBW=`grep -v Th#  out | grep randomWrite | awk '{print $2}'`
    echo "tmpSRBW $tmpSRBW"
    sumSRBW=`echo "$sumSRBW + $tmpSRBW" | bc`
    sumSWBW=`echo "$sumSWBW + $tmpSWBW" | bc`
    sumRRBW=`echo "$sumRRBW + $tmpRRBW" | bc`
    sumRWBW=`echo "$sumRWBW + $tmpRWBW" | bc`
  done

  avgSRBW=`echo "$sumSRBW / $epoch" | bc`
  avgSWBW=`echo "$sumSWBW / $epoch" | bc`
  avgRRBW=`echo "$sumRRBW / $epoch" | bc`
  avgRWBW=`echo "$sumRWBW / $epoch" | bc`

  echo "$thread $avgSRBW $avgSWBW $avgRRBW $avgRWBW" >> $result
done
