#test.sh(mocc)
tuple=200
maxope=10
workload=3
cpumhz=2400
epochtime=40
extime=3
epoch=3

result=result_mocc_r2_tuple200.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./mocc.exe $tuple $maxope thread $workload $cpumhz $epochtime $extime" >> $result
for ((thread = 2; thread <= 24; thread+=2))
do
    sum=0
	echo "./mocc.exe $tuple $maxope $thread $workload $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./mocc.exe $tuple $maxope $thread $workload $cpumhz $epochtime $extime`
        sum=`echo "$sum + $tmp" | bc -l `
        echo "sum: $sum,   tmp: $tmp"

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
result=result_mocc_r2_tuple10000.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./mocc.exe $tuple $maxope thread $workload $cpumhz $epochtime $extime" >> $result
for ((thread = 2; thread <= 24; thread+=2))
do
    sum=0
	echo "./mocc.exe $tuple $maxope $thread $workload $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./mocc.exe $tuple $maxope $thread $workload $cpumhz $epochtime $extime`
        sum=`echo "$sum + $tmp" | bc -l `
        echo "sum: $sum,   tmp: $tmp"

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

