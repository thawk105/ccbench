#test.sh(cicada)
tuple=200
maxope=10
workload=3
wal=OFF
group_commit=OFF
cpu_mhz=2400
io_time_ns=5
group_commit_timeout_us=2
lock_release=E
extime=3
uw=0
epoch=3

result=result_cicada_r2_tuple200.dat
rm $result
echo "#worker thread, throughput, min, max" >> $result
echo "#./cicada.exe $tuple $maxope thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw" >> $result
for ((thread=2; thread<=24; thread+=2))
do
    sum=0
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw"
	echo "$thread $epoch"
 
 	max=0
	min=0	
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw`
        sum=`echo "$sum + $tmp" | bc -l`
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
result=result_cicada_r2_tuple10000.dat
rm $result
echo "#worker thread, throughput, min, max" >> $result
echo "#./cicada.exe $tuple $maxope thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw" >> $result
for ((thread=2; thread<=24; thread+=2))
do
    sum=0
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw"
	echo "$thread $epoch"
 
 	max=0
	min=0	
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime $uw`
        sum=`echo "$sum + $tmp" | bc -l`
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

