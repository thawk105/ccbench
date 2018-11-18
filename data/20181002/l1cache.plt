set logscale x
set logscale y

set xlabel "Number of Tuples"
set key right bottom
set terminal postscript eps enhanced color size 9cm,7cm
set output "silo_l1cache_read-only.eps"
plot "result_silo_r10_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads", "result_silo_r10_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses"

set xlabel "Number of Tuples"
set key right bottom
set terminal postscript eps enhanced color size 9cm,7cm
set output "silo_l1cache_all.eps"
plot "result_silo_r10_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads, read only", "result_silo_r10_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses, read only", "result_silo_r8_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads, read 80\%", "result_silo_r8_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses, read 80\%", "result_silo_r5_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads, read 50\%", "result_silo_r5_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses, read 50\%", "result_silo_r2_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads, read 20\%", "result_silo_r2_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses, read 20\%", "result_silo_r0_L1-dcache-loads.dat" w errorlines title "L1-dcache-loads, read 0\%", "result_silo_r0_L1-dcache-load-misses.dat" w errorlines title "L1-dcache-load-misses, read 0\%"

