set logscale x
set format y "%10.0f"
set yrange[0:100]

set lmargin 8
set ylabel offset 7,0

set xlabel "Number of Tuples"
set ylabel "Cache Miss Ratio [\%]"
set key left top
set terminal postscript eps enhanced color size 20cm,10cm
set output "cicada_cache.eps"
plot "result_cicada_r10_cache.dat" w errorlines title "read only", "result_cicada_r8_cache.dat" w errorlines title "read 80\%", "result_cicada_r5_cache.dat" w errorlines title "read 50\%", "result_cicada_r2_cache.dat" w errorlines title "read 20\%", "result_cicada_r0_cache.dat" w errorlines title "write only", 

set xlabel "Number of Tuples"
set ylabel "Cache Miss Ratio [\%]"
set key left top
set terminal postscript eps enhanced color size 20cm,10cm
set output "ermia_cache.eps"
plot "result_ermia_r10_cache.dat" w errorlines title "read only", "result_ermia_r8_cache.dat" w errorlines title "read 80\%", "result_ermia_r5_cache.dat" w errorlines title "read 50\%", "result_ermia_r2_cache.dat" w errorlines title "read 20\%", "result_ermia_r0_cache.dat" w errorlines title "write only", 

set xlabel "Number of Tuples"
set ylabel "Cache Miss Ratio [\%]"
set key left top
set terminal postscript eps enhanced color size 20cm,10cm
set output "silo_cache.eps"
plot "result_silo_r10_cache.dat" w errorlines title "read only", "result_silo_r8_cache.dat" w errorlines title "read 80\%", "result_silo_r5_cache.dat" w errorlines title "read 50\%", "result_silo_r2_cache.dat" w errorlines title "read 20\%", "result_silo_r0_cache.dat" w errorlines title "write only", 

set xlabel "Number of Tuples"
set ylabel "Cache Miss Ratio [\%]"
set key left top
set terminal postscript eps enhanced color size 20cm,10cm
set output "tictoc_cache.eps"
plot "result_tictoc_r10_cache.dat" w errorlines title "read only", "result_tictoc_r8_cache.dat" w errorlines title "read 80\%", "result_tictoc_r5_cache.dat" w errorlines title "read 50\%", "result_tictoc_r2_cache.dat" w errorlines title "read 20\%", "result_tictoc_r0_cache.dat" w errorlines title "write only", 

set xlabel "Number of Tuples"
set ylabel "Cache Miss Ratio [\%]"
set key left top
set terminal postscript eps enhanced color size 20cm,10cm
set output "ss2pl_cache.eps"
plot "result_ss2pl_r10_cache.dat" w errorlines title "read only", "result_ss2pl_r8_cache.dat" w errorlines title "read 80\%", "result_ss2pl_r5_cache.dat" w errorlines title "read 50\%", "result_ss2pl_r2_cache.dat" w errorlines title "read 20\%", "result_ss2pl_r0_cache.dat" w errorlines title "write only", 
