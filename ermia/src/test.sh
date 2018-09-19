#test.sh(ermia)
tuple=200
maxope=10
pronum=10000
cpu_mhz=2400
extime=3
epoch=3

result=result_ermia_r5_tuple200_ar.dat
rratio=0.5
rm $result
echo "#worker threads, throughput, min, max" >> $result
for ((thread=2; thread<=24; thread+=2))
do
	sum=0
	echo "./ermia.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime"

	max=0
	min=0
	for ((i=1; i <= epoch; ++i))
	do
		tmp=`./ermia.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime`
		sum=`echo "$sum + $tmp" | bc -l`
		echo "sum: $sum,	tmp: $tmp"

		if test $i -eq 1 ; then
			max=$tmp
			min=$tmp
		fi

		flag=`echo "$tmp > $max" | bc -l`
		if test $flag -eq 1 ; then
			max=$tmp
		fi
		flag=`echo "$tmp < $min" | bc -l`
		if test $flag -eq 1 ; then
			min=$tmp
		fi
	done
	avg=`echo "$sum / $epoch" | bc -l`
	echo "sum: $sum, epoch: $epoch"
	echo "avg $avg"
	echo "max: $max"
	echo "min: $min"
	echo "$thread $avg $min $max" >> $result
done

tuple=10000
result=result_ermia_r5_tuple10000_ar.dat
rratio=0.5
rm $result
echo "#worker threads, throughput, min, max" >> $result
for ((thread=2; thread<=24; thread+=2))
do
	sum=0
	echo "./ermia.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime"

	max=0
	min=0
	for ((i=1; i <= epoch; ++i))
	do
		tmp=`./ermia.exe $tuple $maxope $thread $pronum $rratio $cpu_mhz $extime`
		sum=`echo "$sum + $tmp" | bc -l`
		echo "sum: $sum,	tmp: $tmp"

		if test $i -eq 1 ; then
			max=$tmp
			min=$tmp
		fi

		flag=`echo "$tmp > $max" | bc -l`
		if test $flag -eq 1 ; then
			max=$tmp
		fi
		flag=`echo "$tmp < $min" | bc -l`
		if test $flag -eq 1 ; then
			min=$tmp
		fi
	done
	avg=`echo "$sum / $epoch" | bc -l`
	echo "sum: $sum, epoch: $epoch"
	echo "avg $avg"
	echo "max: $max"
	echo "min: $min"
	echo "$thread $avg $min $max" >> $result
done

