#!/bin/bash
DATA="druzice.dat"
gnuplot <<__END__
#
# Model: druzice
#
set output "druzice.pdf"; set term pdf
#set output "druzice.eps"
#set term corel
#set output "druzice.obj"
#set term  tgif [2,2]

set grid
set title "Druzice a 2 hmotna telesa"
set nokey
set xlabel "x [10^3 km]"
set ylabel "y [10^3 km]"
set zlabel "z [10^3 km]"
set tics font ",9"
set style data lines
#set data style dots
plot "$DATA" using 2:3, "" using 5:6
splot "$DATA" using 2:3:4, "" using 5:6:7
__END__
#
