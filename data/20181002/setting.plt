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

set title "tuple 1k, write only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r0.eps"
plot "result_cicada_r0_tuple1k.dat" w errorlines title "Cicada", "result_silo_r0_tuple1k.dat" w errorlines title "Silo", "result_ermia_r0_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple1k.dat" w errorlines title "TicToc",

set title "tuple 1m, write only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r0.eps"
plot "result_cicada_r0_tuple1m.dat" w errorlines title "Cicada", "result_silo_r0_tuple1m.dat" w errorlines title "Silo", "result_ermia_r0_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple1m.dat" w errorlines title "TicToc",

set title "tuple 200, read 20%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r2.eps"
plot "result_cicada_r2_tuple200.dat" w errorlines title "Cicada", "result_silo_r2_tuple200.dat" w errorlines title "Silo", "result_ermia_r2_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple200.dat" w errorlines title "TicToc",

set title "tuple 1k, read 20%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r2.eps"
plot "result_cicada_r2_tuple1k.dat" w errorlines title "Cicada", "result_silo_r2_tuple1k.dat" w errorlines title "Silo", "result_ermia_r2_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple1k.dat" w errorlines title "TicToc",

set title "tuple 1m, read 20%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r2.eps"
plot "result_cicada_r2_tuple1m.dat" w errorlines title "Cicada", "result_silo_r2_tuple1m.dat" w errorlines title "Silo", "result_ermia_r2_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple1m.dat" w errorlines title "TicToc",

set title "tuple 200, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set yrange [0:2000000]
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r5.eps"
plot "result_cicada_r5_tuple200.dat" w errorlines title "Cicada", "result_silo_r5_tuple200.dat" w errorlines title "Silo", "result_ermia_r5_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple200.dat" w errorlines title "TicToc",
set autoscale y

set title "tuple 1k, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key right top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r5.eps"
plot "result_cicada_r5_tuple1k.dat" w errorlines title "Cicada", "result_silo_r5_tuple1k.dat" w errorlines title "Silo", "result_ermia_r5_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple1k.dat" w errorlines title "TicToc",

set title "tuple 1m, read 50%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r5.eps"
plot "result_cicada_r5_tuple1m.dat" w errorlines title "Cicada", "result_silo_r5_tuple1m.dat" w errorlines title "Silo", "result_ermia_r5_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple1m.dat" w errorlines title "TicToc",

set title "tuple 200, read 80%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r8.eps"
plot "result_cicada_r8_tuple200.dat" w errorlines title "Cicada", "result_silo_r8_tuple200.dat" w errorlines title "Silo", "result_ermia_r8_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple200.dat" w errorlines title "TicToc",

set title "tuple 1k, read 80%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r8.eps"
plot "result_cicada_r8_tuple1k.dat" w errorlines title "Cicada", "result_silo_r8_tuple1k.dat" w errorlines title "Silo", "result_ermia_r8_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple1k.dat" w errorlines title "TicToc",

set title "tuple 1m, read 80%"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r8.eps"
plot "result_cicada_r8_tuple1m.dat" w errorlines title "Cicada", "result_silo_r8_tuple1m.dat" w errorlines title "Silo", "result_ermia_r8_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple1m.dat" w errorlines title "TicToc",

set title "tuple 200, read only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r10.eps"
plot "result_cicada_r10_tuple200.dat" w errorlines title "Cicada", "result_silo_r10_tuple200.dat" w errorlines title "Silo", "result_ermia_r10_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple200.dat" w errorlines title "TicToc",

set title "tuple 1k, read only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r10.eps"
plot "result_cicada_r10_tuple1k.dat" w errorlines title "Cicada", "result_silo_r10_tuple1k.dat" w errorlines title "Silo", "result_ermia_r10_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple1k.dat" w errorlines title "TicToc",

set title "tuple 1m, read only"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r10.eps"
plot "result_cicada_r10_tuple1m.dat" w errorlines title "Cicada", "result_silo_r10_tuple1m.dat" w errorlines title "Silo", "result_ermia_r10_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple1m.dat" w errorlines title "TicToc",

set title "Cicada, logging validation"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1m_log_validation.eps"
plot "result_cicada_r5_waloff_tuple1m.dat" w errorlines title "WAL OFF", "result_cicada_r5_pwal_tuple1m.dat" w errorlines title "P-WAL", "result_cicada_r5_swal_tuple1m.dat" w errorlines title "Single-WAL", 

set title "Cicada, logging validation"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1k_log_validation_4pair.eps"
plot "result_cicada_r5_pwal_ElrGrp_tuple1k.dat" w errorlines title "ELR + Group", "result_cicada_r5_pwal_ElrNgrp_tuple1k.dat" w errorlines title "ELR + No group", "result_cicada_r5_pwal_NlrGrp_tuple1k.dat" w errorlines title "CLR + Group", "result_cicada_r5_pwal_NlrNgrp_tuple1k.dat" w errorlines title "CLR + No group", 

set title "Cicada, logging validation"
set xlabel "Number of threads"
set ylabel "Throughput (Trans/Sec)"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1m_log_validation_4pair.eps"
plot "result_cicada_r5_pwal_ElrGrp_tuple1m.dat" w errorlines title "ELR + Group", "result_cicada_r5_pwal_ElrNgrp_tuple1m.dat" w errorlines title "ELR + No group", "result_cicada_r5_pwal_NlrGrp_tuple1m.dat" w errorlines title "CLR + Group", "result_cicada_r5_pwal_NlrNgrp_tuple1m.dat" w errorlines title "CLR + No group", 

