set logscale x
set logscale y

set xlabel "Number of Tuples"
set key right bottom
set terminal postscript eps enhanced color size 9cm,7cm
set output "silo_l1cache.eps"
plot "result_silo_r10_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads", "result_silo_r10_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses"
