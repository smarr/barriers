#!/usr/bin/make -f
# Makefile for Barrier experiments, targets tilera and intel multicore systems
#
# Copyright (c) 2010 Stefan Marr, Vrije Universiteit Brussel
# <http://www.stefan-marr.de/>, <http://code.google.com/p/barriers/>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


# Include the overall barrier configuration
include barrier.mk

# Now define the actual targets

EPCC        = $(addsuffix .epcc,  $(BARRIERS))
EPCC_SIMPLE = $(addsuffix .epcc-simple,  $(BARRIERS))
#RADIX       = $(addsuffix .radix, $(BARRIERS))
DYNAMIC     = $(addsuffix .dynamic, $(DYNAMIC_BARRIERS))
ABSOH       = $(addsuffix .absoh, $(BARRIERS))
LIOH       = $(addsuffix .lioh, $(BARRIERS))
ALL_TARGETS = $(EPCC) $(RADIX) $(DYNAMIC) $(EPCC_SIMPLE) $(ABSOH) $(LIOH)

all: $(ALL_TARGETS)

# the barrier target
%.epcc: barrier.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include barriers/$*.h barrier.cpp $(LIBS) $(LFLAGS) -o $@

%.epcc-simple: barrier.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DUSE_TWO_PHASE=0 -include barriers/$*.h barrier.cpp $(LIBS) $(LFLAGS) -o $@

%.dynamic: bench/dynamic.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include barriers/$*.h bench/dynamic.cpp $(LIBS) $(LFLAGS) -o $@

%.absoh: bench/absoh.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include barriers/$*.h bench/absoh.cpp $(LIBS) $(LFLAGS) -o $@

%.lioh: bench/lioh.cpp Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include barriers/$*.h bench/lioh.cpp $(LIBS) $(LFLAGS) -o $@

%.radix:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -include barriers/$*.h bench/radix.cpp $(LIBS) $(LFLAGS) -o $@

main: main.o Makefile
	$(CXX) $@.o $(LIBS) $(LFLAGS) -o $@

info:
	@echo Platform: $(PLATFORM)
	@echo CXX:      $(CXX)
	@echo Threads:  $(NUM_PARTICIPANTS)
	@echo OPT:      $(OPT)
	@echo LFLAGS:   $(LFLAGS)

clean:
	$(RM) $(ALL_TARGETS) *.o

distclean: clean
	$(RM) *.gcda *.gcno

.PHONY: clean distclean all info
