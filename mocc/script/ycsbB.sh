#ycsbB.sh(mocc)
maxope=10
rratio=95
skew=0
ycsb=ON
cpumhz=2400
epochtime=40
extime=3
epoch=5

host=`hostname`
chris41="chris41.omni.hpcc.jp"
dbs11="dbs11"

tuple=500
result=result_mocc_ycsbB_tuple500.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./mocc.exe $tuple $maxope thread $rratio $skew $ycsb $cpumhz $epochtime $extime" >> $result

thread=2
sum=0
echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"

max=0
min=0
for ((i = 1; i <= epoch; ++i))
do
    tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

if  test $host = $chris41 ; then
inith=4
enth=24
inc=4
fi
if  test $host = $dbs11 ; then
inith=16
enth=224
inc=16
fi
for ((thread=$inith; thread<=$enth; thread+=$inc))
do
    sum=0
	echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
	thout=`echo "$thread - 1" | bc`
	echo "$thout $avg $min $max" >> $result
done

tuple=500000
result=result_mocc_ycsbB_tuple500k.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./mocc.exe $tuple $maxope thread $rratio $skew $ycsb $cpumhz $epochtime $extime" >> $result

thread=2
sum=0
echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"

max=0
min=0
for ((i = 1; i <= epoch; ++i))
do
    tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

if  test $host = $chris41 ; then
inith=4
enth=24
inc=4
fi
if  test $host = $dbs11 ; then
inith=16
enth=224
inc=16
fi
for ((thread=$inith; thread<=$enth; thread+=$inc))
do
    sum=0
	echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
	thout=`echo "$thread - 1" | bc`
	echo "$thout $avg $min $max" >> $result
done

tuple=5000000
result=result_mocc_ycsbB_tuple5m.dat
rm $result
echo "#worker threads, throughput, min, max" >> $result
echo "#./mocc.exe $tuple $maxope thread $rratio $skew $ycsb $cpumhz $epochtime $extime" >> $result

thread=2
sum=0
echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"

max=0
min=0
for ((i = 1; i <= epoch; ++i))
do
    tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
thout=`echo "$thread - 1" | bc`
echo "$thout $avg $min $max" >> $result

if  test $host = $chris41 ; then
inith=4
enth=24
inc=4
fi
if  test $host = $dbs11 ; then
inith=16
enth=224
inc=16
fi
for ((thread=$inith; thread<=$enth; thread+=$inc))
do
    sum=0
	echo "./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime"
    
	max=0
	min=0
    for ((i = 1; i <= epoch; ++i))
    do
        tmp=`./mocc.exe $tuple $maxope $thread $rratio $skew $ycsb $cpumhz $epochtime $extime`
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
	thout=`echo "$thread - 1" | bc`
	echo "$thout $avg $min $max" >> $result
done

