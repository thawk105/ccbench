set xlabel "Number of threads"
set xlabel offset 0,0.5

set ylabel "Throughput (tps)"
set ylabel offset 1,0

set xlabel font "Courier,12"
set ylabel font "Courier,12"
set tics   font "Courier,12"
set key    font "Courier,12"

set lmargin 8
set bmargin 3

set key horiz outside center top box

set format y "%2.0t{/Symbol \264}10^{%T}"
set notitle

set autoscale y

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r0.eps"
plot "result_cicada_r0_tuple200.dat" w errorlines title "Cicada", "result_silo_r0_tuple200.dat" w errorlines title "Silo", "result_ermia_r0_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple200.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r0.eps"
plot "result_cicada_r0_tuple1k.dat" w errorlines title "Cicada", "result_silo_r0_tuple1k.dat" w errorlines title "Silo", "result_ermia_r0_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple1k.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r0.eps"
plot "result_cicada_r0_tuple1m.dat" w errorlines title "Cicada", "result_silo_r0_tuple1m.dat" w errorlines title "Silo", "result_ermia_r0_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r0_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r0_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r0_tuple1m.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r2.eps"
plot "result_cicada_r2_tuple200.dat" w errorlines title "Cicada", "result_silo_r2_tuple200.dat" w errorlines title "Silo", "result_ermia_r2_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple200.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r2.eps"
plot "result_cicada_r2_tuple1k.dat" w errorlines title "Cicada", "result_silo_r2_tuple1k.dat" w errorlines title "Silo", "result_ermia_r2_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple1k.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r2.eps"
plot "result_cicada_r2_tuple1m.dat" w errorlines title "Cicada", "result_silo_r2_tuple1m.dat" w errorlines title "Silo", "result_ermia_r2_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r2_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r2_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r2_tuple1m.dat" w errorlines title "TicToc",

set ytics 10e5
set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r5.eps"
plot "result_cicada_r5_tuple200.dat" w errorlines title "Cicada", "result_silo_r5_tuple200.dat" w errorlines title "Silo", "result_ermia_r5_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple200.dat" w errorlines title "TicToc",
set ytics autofreq

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r5.eps"
plot "result_cicada_r5_tuple1k.dat" w errorlines title "Cicada", "result_silo_r5_tuple1k.dat" w errorlines title "Silo", "result_ermia_r5_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple1k.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r5.eps"
plot "result_cicada_r5_tuple1m.dat" w errorlines title "Cicada", "result_silo_r5_tuple1m.dat" w errorlines title "Silo", "result_ermia_r5_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r5_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r5_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r5_tuple1m.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r8.eps"
plot "result_cicada_r8_tuple200.dat" w errorlines title "Cicada", "result_silo_r8_tuple200.dat" w errorlines title "Silo", "result_ermia_r8_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple200.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r8.eps"
plot "result_cicada_r8_tuple1k.dat" w errorlines title "Cicada", "result_silo_r8_tuple1k.dat" w errorlines title "Silo", "result_ermia_r8_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple1k.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r8.eps"
plot "result_cicada_r8_tuple1m.dat" w errorlines title "Cicada", "result_silo_r8_tuple1m.dat" w errorlines title "Silo", "result_ermia_r8_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r8_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r8_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r8_tuple1m.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple200_r10.eps"
plot "result_cicada_r10_tuple200.dat" w errorlines title "Cicada", "result_silo_r10_tuple200.dat" w errorlines title "Silo", "result_ermia_r10_tuple200.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple200.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple200.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple200.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1k_r10.eps"
plot "result_cicada_r10_tuple1k.dat" w errorlines title "Cicada", "result_silo_r10_tuple1k.dat" w errorlines title "Silo", "result_ermia_r10_tuple1k.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple1k.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple1k.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple1k.dat" w errorlines title "TicToc",

set terminal postscript eps enhanced color size 6cm,3cm
set output "comp_cicada_silo_ermia_ss2pl_mocc_tictoc_tuple1m_r10.eps"
plot "result_cicada_r10_tuple1m.dat" w errorlines title "Cicada", "result_silo_r10_tuple1m.dat" w errorlines title "Silo", "result_ermia_r10_tuple1m.dat" w errorlines title "ERMIA", "result_ss2pl_r10_tuple1m.dat" w errorlines title "SS2PL", "result_mocc_r10_tuple1m.dat" w errorlines title "MOCC", "result_tictoc_r10_tuple1m.dat" w errorlines title "TicToc",

