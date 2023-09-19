#################################################################
# Makefile for SIMLIB (unix)

MAKEFILE = Makefile.`uname -s`

#current-debuging-target: 32

.PHONY: all 32 64 fuzzy test test32 install uninstall cppcheck clean pack

#################################################################
all:
	gmake -j3 -f $(MAKEFILE)

# special versions for 32/64 bit platforms
# maybe you need gmake clean first...
32:
	gmake -f $(MAKEFILE) EXTRA_CXXFLAGS="-m32"
64:
	gmake -f $(MAKEFILE) EXTRA_CXXFLAGS="-m64"

#TODO: change
fuzzy:
	gmake -f $(MAKEFILE)  MODULES="fuzzy" fuzzymodule all

#################################################################
dep:
	gmake -f $(MAKEFILE) dep
#################################################################
test:
	gmake -f $(MAKEFILE) test
test32:
	gmake -f $(MAKEFILE) EXTRA_CXXFLAGS="-m32" test
#################################################################
install:
	gmake -f $(MAKEFILE) install
uninstall:
	gmake -f $(MAKEFILE) uninstall

#############################################################################
cppcheck:
	cppcheck --force --enable=all . 2>00-cppcheck-log

#################################################################
clean:
	gmake -f $(MAKEFILE) clean
	gmake -f $(MAKEFILE) MODULES="fuzzy" clean
pack:
	gmake -f $(MAKEFILE) pack
#################################################################
