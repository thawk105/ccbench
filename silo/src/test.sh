#test.sh(silo)
tuple=200
maxope=10
pronum=10000
cpumhz=2400
epochtime=40
extime=3
epoch=3

rratio=0.5
result=result_silo_r5_tuple200.dat
rm $result
echo "#worker threads, abort_rate, min, max" >> $result
for ((thread = 2; thread <= 24; thread+=2))
do
    sum=0
	echo "./silo.exe $tuple $maxope $thread $pronum $rratio $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./silo.exe $tuple $maxope $thread $pronum $rratio $cpumhz $epochtime $extime`
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
rratio=0.5
result=result_silo_r5_tuple10000.dat
rm $result
echo "#worker threads, abort_rate, min, max" >> $result
for ((thread = 2; thread <= 24; thread+=2))
do
    sum=0
	echo "./silo.exe $tuple $maxope $thread $pronum $rratio $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./silo.exe $tuple $maxope $thread $pronum $rratio $cpumhz $epochtime $extime`
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

