#test.sh(nocc)
tuple=200
maxope=10
pronum=10000
cpu_mhz=2400
extime=3
epoch=5

result=result_nocc_r2_tuple200.dat
rratio=0.2
rm $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime"
	echo "$thread $epoch"

	for ((i=1; i <= epoch; i++))
	do
		tmp=`./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime`
		sum=`echo "$sum + $tmp" | bc -l`
		echo "sum: $sum,	tmp: $tmp"
	done
	echo -n "$thread " >> $result
	echo "sum:	$sum, epoch: $epoch"
	echo "$sum / $epoch" | bc -l >> $result
	avg=`echo "$sum / $epoch" | bc -l`
	echo "avg $avg"
done

result=result_nocc_r5_tuple200.dat
rratio=0.5
rm $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime"
	echo "$thread $epoch"

	for ((i=1; i <= epoch; i++))
	do
		tmp=`./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime`
		sum=`echo "$sum + $tmp" | bc -l`
		echo "sum: $sum,	tmp: $tmp"
	done
	echo -n "$thread " >> $result
	echo "sum:	$sum, epoch: $epoch"
	echo "$sum / $epoch" | bc -l >> $result
	avg=`echo "$sum / $epoch" | bc -l`
	echo "avg $avg"
done

result=result_nocc_r8_tuple200.dat
rratio=0.8
rm $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime"
	echo "$thread $epoch"

	for ((i=1; i <= epoch; i++))
	do
		tmp=`./nocc.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime`
		sum=`echo "$sum + $tmp" | bc -l`
		echo "sum: $sum,	tmp: $tmp"
	done
	echo -n "$thread " >> $result
	echo "sum:	$sum, epoch: $epoch"
	echo "$sum / $epoch" | bc -l >> $result
	avg=`echo "$sum / $epoch" | bc -l`
	echo "avg $avg"
done

