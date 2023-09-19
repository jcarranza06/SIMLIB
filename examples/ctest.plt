############################################################################
# GNUplot script for model ctest
#
# Data format: Time y
#
reset  # defaults

#set output "ctest.ps"; set term postscript
#set output "ctest.pdf"; set term pdf

set grid
set style data lines

set title "ctest - continuous system response"
set key
set xlabel "Time"
set ylabel "y"
plot [][0:0.07] "ctest.dat" title "y"
pause 1

#end
