set xlabel "#Threads"
set ylabel "Throughput (Trans/Sec)"

set format y "%10.5f"
set format y "%2.2t{/Symbol \264}10^{%T}"

set title "tuple 200, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_tuple200_r5.eps"
plot "result_cicada_r5_tuple200.dat" w lp title "Cicada", "result_silo_r5_tuple200.dat" w lp title "Silo", "result_ermia_r5_tuple200.dat" w lp title "ERMIA", "result_ermia-latchFreeSSN_r5_tuple200.dat" w lp title "ERMIA - latchFree", "result_ss2pl_r5_tuple200.dat" w lp title "SS2PL"

set title "tuple 10k, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_tuple10000_r5.eps"
plot "result_cicada_r5_tuple10000.dat" w lp title "Cicada", "result_silo_r5_tuple10000.dat" w lp title "Silo", "result_ermia-serialSSN_r5_tuple10000.dat" w lp title "ERMIA-serialSSN", "result_ermia-latchFreeSSN_r5_tuple10000.dat" w lp title "ERMIA-latchFreeSSN", "result_ss2pl_r5_tuple10000.dat" using 1:2 w lp title "SS2PL"

set title "SS2PL"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set y2label "Abort ratio"
set y2label offset -2,0
set y2range[0:1]
set y2tics
set terminal postscript eps enhanced color size 9cm,7cm
set output "ss2pl.eps"
set key right bottom
plot "result_ss2pl_r5_tuple200.dat" axis x1y1 w lp title "tuple200 - Th", "result_ss2pl_r5_tuple200_ar.dat" using 1:2:3:4 axis x1y2 w lp title "tuple200 - AR", "result_ss2pl_r5_tuple10000.dat" axis x1y1 w lp title "tuple10k - Th", "result_ss2pl_r5_tuple10000_ar.dat" using 1:2:3:4 axis x1y2 w lp title "tuple10k - AR"
