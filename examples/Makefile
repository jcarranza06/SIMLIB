#
#  Makefile for SIMLIB
#  ===================
#

# C++ compiler flags
CXXFLAGS = -Wall
CXXFLAGS += -std=c++11
CXXFLAGS += -O2 # add optimization level
CXXFLAGS += -g  # add debug info
#CXXFLAGS += -pg # add profiling support
#CXXFLAGS += -Weffc++ # add extra checking of source code

MAKEFILE = Makefile.generic

.PHONY: all 32 64 clean clean-all plot run pack

#################################################################
all:
	CXXFLAGS="$(CXXFLAGS)" gmake -j3 -f $(MAKEFILE)

# special versions for 32/64bit version, do "make clean" first
32:
	CXXFLAGS="$(CXXFLAGS) -m32" gmake -j3 -f $(MAKEFILE)

64:
	CXXFLAGS="$(CXXFLAGS) -m64" gmake -j3 -f $(MAKEFILE)

#################################################################
clean:
	gmake -f $(MAKEFILE) clean-all

plot: all
	gmake -f $(MAKEFILE) plot

run: all
	gmake -f $(MAKEFILE) run

pack: clean
	gmake -f $(MAKEFILE) pack

#################################################################
