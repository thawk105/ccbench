
result=result_masstree_put.dat
rm $result
epoch=3
thread=1

sumTH=0
avgTH=0
tmpTH=0
for ((i = 1; i <= epoch; ++i))
do
  ../unit-mt $thread > out
  tmpTH=`cat out`
  sumTH=`echo "$sumTH + $tmpTH" | bc`
done

avgTH=`echo "$sumTH / $epoch" | bc`
echo "$thread $avgTH" | tee -a $result

for ((thread = 28; thread <= 224; thread+=28))
do

  sumTH=0
  avgTH=0
  tmpTH=0
  for ((i = 1; i <= epoch; ++i))
  do
    ../unit-mt $thread > out
    tmpTH=`cat out`
    sumTH=`echo "$sumTH + $tmpTH" | bc`
  done
  
  avgTH=`echo "$sumTH / $epoch" | bc`
  echo "$thread $avgTH" | tee -a $result

done

