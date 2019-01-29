#ycsbA.sh(ss2pl)
maxope=10
rratio=50
skew=0
ycsb=ON
cpu_mhz=2400
extime=3
epoch=5

tuple=500
result=result_ss2pl_ycsbA_tuple500.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result

thread=1
sum=0
echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
echo "$thread $epoch"

max=0
min=0
for ((i=1; i <= epoch; i++))
do
	tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
echo "sum:	$sum, epoch: $epoch"
echo "avg $avg"
echo "max: $max"
echo "min: $min"
echo "$thread $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
	echo "sum:	$sum, epoch: $epoch"
	echo "avg $avg"
	echo "max: $max"
	echo "min: $min"
	echo "$thread $avg $min $max" >> $result
done

tuple=500000
result=result_ss2pl_ycsbA_tuple500k.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result

thread=1
sum=0
echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
echo "$thread $epoch"

max=0
min=0
for ((i=1; i <= epoch; i++))
do
	tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
echo "sum:	$sum, epoch: $epoch"
echo "avg $avg"
echo "max: $max"
echo "min: $min"
echo "$thread $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
	echo "sum:	$sum, epoch: $epoch"
	echo "avg $avg"
	echo "max: $max"
	echo "min: $min"
	echo "$thread $avg $min $max" >> $result
done

tuple=5000000
result=result_ss2pl_ycsbA_tuple5m.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result

thread=1
sum=0
echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
echo "$thread $epoch"

max=0
min=0
for ((i=1; i <= epoch; i++))
do
	tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
echo "sum:	$sum, epoch: $epoch"
echo "avg $avg"
echo "max: $max"
echo "min: $min"
echo "$thread $avg $min $max" >> $result

for ((thread=4; thread<=24; thread+=4))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $rratio $skew $ycsb $cpu_mhz $extime`
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
	echo "sum:	$sum, epoch: $epoch"
	echo "avg $avg"
	echo "max: $max"
	echo "min: $min"
	echo "$thread $avg $min $max" >> $result
done

