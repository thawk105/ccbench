set xlabel "#Threads"
set ylabel "Throughput (Trans/Sec)"

set format y "%10.5f"
set format y "%2.2t{/Symbol \264}10^{%T}"

set title "tuple 200, write only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r0.eps"
plot "result_cicada_r0_tuple200.dat" w errorlines title "Cicada", "result_silo_r0_tuple200.dat" w errorlines title "Silo", "result_ermia_r0_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple200.dat" w errorlines title "TicToc",

set title "tuple 10k, write only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple10000_r0.eps"
plot "result_cicada_r0_tuple10000.dat" w errorlines title "Cicada", "result_silo_r0_tuple10000.dat" w errorlines title "Silo", "result_ermia_r0_tuple10000.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple10000.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple10000.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple10000.dat" w errorlines title "TicToc",

set title "tuple 200, read 20%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r2.eps"
plot "result_cicada_r2_tuple200.dat" w errorlines title "Cicada", "result_silo_r2_tuple200.dat" w errorlines title "Silo", "result_ermia_r2_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple200.dat" w errorlines title "TicToc",

set title "tuple 10k, read 20%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple10000_r2.eps"
plot "result_cicada_r2_tuple10000.dat" w errorlines title "Cicada", "result_silo_r2_tuple10000.dat" w errorlines title "Silo", "result_ermia_r2_tuple10000.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple10000.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple10000.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple10000.dat" w errorlines title "TicToc",

set title "tuple 200, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r5.eps"
plot "result_cicada_r5_tuple200.dat" w errorlines title "Cicada", "result_silo_r5_tuple200.dat" w errorlines title "Silo", "result_ermia_r5_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple200.dat" w errorlines title "TicToc",

set title "tuple 10k, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple10000_r5.eps"
plot "result_cicada_r5_tuple10000.dat" w errorlines title "Cicada", "result_silo_r5_tuple10000.dat" w errorlines title "Silo", "result_ermia_r5_tuple10000.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple10000.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple10000.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple10000.dat" w errorlines title "TicToc",

set title "tuple 200, read 80%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r8.eps"
plot "result_cicada_r8_tuple200.dat" w errorlines title "Cicada", "result_silo_r8_tuple200.dat" w errorlines title "Silo", "result_ermia_r8_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple200.dat" w errorlines title "TicToc",

set title "tuple 10k, read 80%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple10000_r8.eps"
plot "result_cicada_r8_tuple10000.dat" w errorlines title "Cicada", "result_silo_r8_tuple10000.dat" w errorlines title "Silo", "result_ermia_r8_tuple10000.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple10000.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple10000.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple10000.dat" w errorlines title "TicToc",

set title "tuple 200, read only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r10.eps"
plot "result_cicada_r10_tuple200.dat" w errorlines title "Cicada", "result_silo_r10_tuple200.dat" w errorlines title "Silo", "result_ermia_r10_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple200.dat" w errorlines title "TicToc",

set title "tuple 10k, read only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple10000_r10.eps"
plot "result_cicada_r10_tuple10000.dat" w errorlines title "Cicada", "result_silo_r10_tuple10000.dat" w errorlines title "Silo", "result_ermia_r10_tuple10000.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple10000.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple10000.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple10000.dat" w errorlines title "TicToc",

set title "Cicada, IO 50us"
set xlabel "Number of threads"
set ylabel "Throughput (trans/sec)"
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_wal_experiment.eps"
set key left top
plot "result_cicada_r5_tuple10000.dat" w errorlines title "WAL OFF", "result_cicada_r5_pwal_io50us.dat" w errorlines title "P-WAL", "result_cicada_r5_swal_io50us.dat" w errorlines title "Single WAL"

set title "SS2PL"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set terminal postscript eps enhanced color size 9cm,7cm
set output "ss2pl_throughput_modify_tuple200.eps"
set key left top
plot "result_ss2pl_r5_tuple200.dat" w errorlines title "tuple 200 - 4bytes", "result_ss2pl_r5_modify100x_tuple200.dat" w errorlines title "tuple 200 - 400bytes"

set title "SS2PL"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set terminal postscript eps enhanced color size 9cm,7cm
set output "ss2pl_throughput_modify_tuple10000.eps"
set key left top
plot "result_ss2pl_r5_tuple10000.dat" w errorlines title "tuple 10000 - 4bytes", "result_ss2pl_r5_modify100x_tuple10000.dat" w errorlines title "tuple 10000 - 400bytes"

set title "SS2PL"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set y2label "Abort ratio"
set y2label offset -2,0
set y2range[0:1]
set y2tics
set terminal postscript eps enhanced color size 9cm,7cm
set output "ss2pl_throughput_ar.eps"
set key right bottom
plot "result_ss2pl_r5_tuple200.dat" axis x1y1 w errorlines title "tuple200 - Th", "result_ss2pl_r5_tuple200_ar.dat" using 1:2:3:4 axis x1y2 w errorlines title "tuple200 - AR", "result_ss2pl_r5_tuple10000.dat" axis x1y1 w errorlines title "tuple10k - Th", "result_ss2pl_r5_tuple10000_ar.dat" using 1:2:3:4 axis x1y2 w errorlines title "tuple10k - AR"

set format y "%2.1f"

set title "tuple 200, read 50%"
set xlabel "Number of threads"
set ylabel "Abort ratio"
set yrange[0:1]
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_tuple200_r5_ar.eps"
set key right bottom
plot "result_cicada_r5_tuple200_ar.dat" using 1:2 axis x1y1 w errorlines title "cicada", "result_silo_r5_tuple200_ar.dat" using 1:2 axis x1y1 w errorlines title "Silo", "result_ss2pl_r5_tuple200_ar.dat" using 1:2 axis x1y1 w errorlines title "SS2PL", "result_ermia-latchFreeSSN_r5_tuple200_ar.dat" using 1:2 axis x1y1 w errorlines title "ERMIA+latchFreeSSN"

set title "tuple 10000, read 50%"
set xlabel "Number of threads"
set ylabel "Abort ratio"
set yrange[0:1]
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_tuple10000_r5_ar.eps"
set key left top
plot "result_cicada_r5_tuple10000_ar.dat" using 1:2 axis x1y1 w lp title "cicada", "result_silo_r5_tuple10000_ar.dat" using 1:2 axis x1y1 w lp title "Silo", "result_ss2pl_r5_tuple10000_ar.dat" using 1:2 axis x1y1 w lp title "SS2PL", "result_ermia-latchFreeSSN_r5_tuple10000_ar.dat" using 1:2 axis x1y1 w lp title "ERMIA+latchFreeSSN",

