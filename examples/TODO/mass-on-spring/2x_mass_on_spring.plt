############################################################################

set grid
set style data lines

set title "2x mass on spring"
set key
set xlabel "Time [s]"
set ylabel "y1 [m],  y2 [m]"
plot "test.dat" title "y1", \
     "test.dat" using 1:3 title "y2"
pause -1

set title "phase space"
set nokey
set xlabel "y1 [m]"
set ylabel "y2 [m]"
plot "test.dat" using 2:3 with lines
pause -1

#end
