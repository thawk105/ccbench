#test_cache_ana.sh(cicada)
maxope=10
thread=24
wal=OFF
group_commit=OFF
cpu_mhz=2400
io_time_ns=5
group_commit_timeout_us=2
lock_release=E
extime=3
epoch=5

tuple=100
workload=0
result=result_cicada_r10_cache.dat
rm $result
echo "#tuple, cache-miss-ratio, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=100000000; tuple=$tuple * 10))
do
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
	sum=0
	max=0
	min=0	
	for ((i = 1; i <= epoch; ++i))
	do
	    perf stat -e cache-references,cache-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
	    tmp=`grep cache-misses ./cicada_cache_ana.txt | awk '{print $4}'`
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
	echo "$tuple $avg $min $max" >> $result
done

tuple=100
workload=1
result=result_cicada_r8_cache.dat
rm $result
echo "#tuple, cache-miss-ratio, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=100000000; tuple=$tuple * 10))
do
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
	sum=0
	max=0
	min=0	
	for ((i = 1; i <= epoch; ++i))
	do
	    perf stat -e cache-references,cache-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
	    tmp=`grep cache-misses ./cicada_cache_ana.txt | awk '{print $4}'`
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
	echo "$tuple $avg $min $max" >> $result
done

tuple=100
workload=2
result=result_cicada_r5_cache.dat
rm $result
echo "#tuple, cache-miss-ratio, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=100000000; tuple=$tuple * 10))
do
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
	sum=0
	max=0
	min=0	
	for ((i = 1; i <= epoch; ++i))
	do
	    perf stat -e cache-references,cache-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
	    tmp=`grep cache-misses ./cicada_cache_ana.txt | awk '{print $4}'`
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
	echo "$tuple $avg $min $max" >> $result
done

tuple=100
workload=3
result=result_cicada_r2_cache.dat
rm $result
echo "#tuple, cache-miss-ratio, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=100000000; tuple=$tuple * 10))
do
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
	sum=0
	max=0
	min=0	
	for ((i = 1; i <= epoch; ++i))
	do
	    perf stat -e cache-references,cache-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
	    tmp=`grep cache-misses ./cicada_cache_ana.txt | awk '{print $4}'`
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
	echo "$tuple $avg $min $max" >> $result
done

tuple=100
workload=4
result=result_cicada_r0_cache.dat
rm $result
echo "#tuple, cache-miss-ratio, min, max" >> $result
echo "#./cicada.exe tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime" >> $result

for ((tuple=100; tuple<=100000000; tuple=$tuple * 10))
do
	echo "./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime"
	sum=0
	max=0
	min=0	
	for ((i = 1; i <= epoch; ++i))
	do
	    perf stat -e cache-references,cache-misses -o cicada_cache_ana.txt ./cicada.exe $tuple $maxope $thread $workload $wal $group_commit $cpu_mhz $io_time_ns $group_commit_timeout_us $lock_release $extime
	    tmp=`grep cache-misses ./cicada_cache_ana.txt | awk '{print $4}'`
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
	echo "$tuple $avg $min $max" >> $result
done

