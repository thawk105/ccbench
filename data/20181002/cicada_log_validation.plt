set xlabel "#Threads"
set ylabel "Throughput [Trans/Sec]"

set format y "%2.2t{/Symbol \264}10^{%T}"

set lmargin 12
set bmargin 4
set xlabel offset 0,0

set xlabel "Number of threads"
set ylabel "Throughput [Trans/Sec]"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1m_log_validation.eps"
plot "result_cicada_r5_waloff_tuple1m.dat" w errorlines title "WAL OFF", "result_cicada_r5_pwal_tuple1m.dat" w errorlines title "P-WAL", "result_cicada_r5_swal_tuple1m.dat" w errorlines title "Single-WAL", 

set xlabel "Number of threads"
set ylabel "Throughput [Trans/Sec]"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1k_log_validation_4pair.eps"
plot "result_cicada_r5_pwal_ElrGrp_tuple1k.dat" w errorlines title "ELR + Group", "result_cicada_r5_pwal_ElrNgrp_tuple1k.dat" w errorlines title "ELR + No group", "result_cicada_r5_pwal_NlrGrp_tuple1k.dat" w errorlines title "CLR + Group", "result_cicada_r5_pwal_NlrNgrp_tuple1k.dat" w errorlines title "CLR + No group", 

set xlabel "Number of threads"
set ylabel "Throughput [Trans/Sec]"
set key left top
set terminal postscript eps enhanced color size 9cm,7cm
set output "cicada_tuple1m_log_validation_4pair.eps"
plot "result_cicada_r5_pwal_ElrGrp_tuple1m.dat" w errorlines title "ELR + Group", "result_cicada_r5_pwal_ElrNgrp_tuple1m.dat" w errorlines title "ELR + No group", "result_cicada_r5_pwal_NlrGrp_tuple1m.dat" w errorlines title "CLR + Group", "result_cicada_r5_pwal_NlrNgrp_tuple1m.dat" w errorlines title "CLR + No group", 
