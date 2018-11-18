set logscale x
set logscale y

set xlabel "Number of Read Request"
set key right bottom
set terminal postscript eps enhanced color size 9cm,7cm
set output "ermia_r0-r10_cache-references-misses.eps"
plot "result_ermia_r0-10_cache-references.dat" w errorlines title "cache-references", "result_ermia_r0-10_cache-misses.dat" w errorlines title "cache-misses"

