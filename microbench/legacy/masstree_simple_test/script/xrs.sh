
result=result_masstree_put_xrs.dat
rm $result
epoch=3
thread=224
rs=1000

sumTH=0
avgTH=0
tmpTH=0
for ((i = 1; i <= epoch; ++i))
do
  ../unit-mt $thread $rs > out
  tmpTH=`cat out`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
done

avgTH=`echo "$sumTH / $epoch" | bc`
echo "$rs $avgTH" | tee -a $result

for ((rs = 10000; rs <= 1000000000; rs *= 10))
do

  sumTH=0
  avgTH=0
  tmpTH=0
  for ((i = 1; i <= epoch; ++i))
  do
    ../unit-mt $thread $rs > out
    tmpTH=`cat out`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
  done
  
  avgTH=`echo "$sumTH / $epoch" | bc`
  echo "$rs $avgTH" | tee -a $result

done

