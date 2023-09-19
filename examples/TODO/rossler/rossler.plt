
set output "rossler.pdf"; set term pdfcairo font "sans,7";
set grid
set key opaque at graph 0.98,0.89
set style data lines

A="`grep '^# a' rossler.dat | sed 's/^.* a *= \([-+e\.0-9]*\).*$/\1/'`"
B="`grep '^# b' rossler.dat | sed 's/^.* b *= \([-+e\.0-9]*\).*$/\1/'`"
C="`grep '^# c' rossler.dat | sed 's/^.* c *= \([-+e\.0-9]*\).*$/\1/'`"
T=sprintf("Rossler (a=%s, b=%s, c=%s)", A,B,C)
set title T
#set nokey
set xlabel "t [s]"
set ylabel "x,y,z"
plot [][] "rossler.dat" using 1:2 lw 1.0 title "x" \
          , "" using 1:3 lw 1.2 title "y"         \
          , "" using 1:4 lw 1.0 title "z"   

set title "Rossler equation (phase plane)"
set xlabel "x"
set ylabel "y"
set nokey
plot "rossler.dat" using 2:3

set title "Rossler equation (phase space)"
set xlabel "x"
set ylabel "y"
set zlabel "z"
splot "rossler.dat" using 2:3:4 lw 0.8

