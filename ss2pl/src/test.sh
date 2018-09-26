#test.sh(ss2pl)
maxope=10
cpu_mhz=2400
extime=3
epoch=3

workload=0
tuple=200
result=result_ss2pl_r10_tuple200.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=0
tuple=10000
result=result_ss2pl_r10_tuple10000.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=1
tuple=200
result=result_ss2pl_r8_tuple200.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=1
tuple=10000
result=result_ss2pl_r8_tuple10000.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=2
tuple=200
result=result_ss2pl_r5_tuple200.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=2
tuple=10000
result=result_ss2pl_r5_tuple10000.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=3
tuple=200
result=result_ss2pl_r2_tuple200.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=3
tuple=10000
result=result_ss2pl_r2_tuple10000.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=4
tuple=200
result=result_ss2pl_r0_tuple200.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

workload=4
tuple=10000
result=result_ss2pl_r0_tuple10000.dat
rm $result
echo "#Worker threads, throughput, min, max" >> $result
for ((thread=1; thread<=24; thread+=1))
do
	sum=0
	echo "./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime"
	echo "$thread $epoch"

	max=0
	min=0
	for ((i=1; i <= epoch; i++))
	do
		tmp=`./ss2pl.exe $tuple $maxope $thread $workload $cpu_mhz $extime`
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

