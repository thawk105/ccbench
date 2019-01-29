#ycsbA.sh(silo)
maxope=10
thread=24
rratio=50
skew=0
ycsb=ON
cpumhz=2400
epochtime=40
extime=3
epoch=5

result=result_silo_ycsbA_tuple100-10m.dat
rm $result
echo "#tuple num, abort_rate, min, max" >> $result
echo "#./silo.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime" >> $result

tuple=100
sum=0
echo "./silo.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"

max=0
min=0
for ((i = 1; i <= epoch; ++i))
do
    tmp=`./silo.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
echo "$tuple $avg $min $max" >> $result

for ((tuple = 100; tuple <= 10000000; tuple*=10))
do
    sum=0
	echo "./silo.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./silo.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
	echo "$tuple $avg $min $max" >> $result
done

