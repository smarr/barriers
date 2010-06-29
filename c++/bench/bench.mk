# Makefile for Barrier experiments: Build the SPLASH2 core benchmarks
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

# first include the overall build system configuration, including the barriers
include ../../barrier.mk

# set SPLASH specific flags and necessary libraries
CFLAGS += -I../../
MACROS = ../splash2.m4
M4 = m4 -s -Ulen -Uindex

LDFLAGS += -lpthread 

TARGETS = $(addsuffix .$(TARGET), $(BARRIERS))

x = *

OBJS += starter.o

.DEFAULT_GOAL := all

all: $(TARGETS)

clean:
	rm -rf $(CLEAN)

%.$(TARGET): $(OBJS) %-barrier.o 
	$(CXX) $(OBJS) $*-barrier.o $(CFLAGS) $(OPT) -o $@ $(LDFLAGS) $(LIBS)


%-barrier.o:
	$(CXX) $(CFLAGS) -include $(CURDIR)/../../barriers/$*.h $(CURDIR)/../../barriers/barrier.cpp -c -o $*-barrier.o

starter.o:
	$(CC) $(CFLAGS) ../starter.c -c -o $@

#do not remove intermediate files
.SECONDARY:

info:
	@echo Platform: $(PLATFORM)
	@echo CXX:      $(CXX)
	@echo Threads:  $(NUM_PARTICIPANTS)
	@echo OPT:      $(OPT)
	@echo LFLAGS:   $(LFLAGS)
	@echo Target:   $(TARGET)
	@echo Targets:  $(TARGETS)

.SUFFIXES:
.SUFFIXES:  .o .c .tplC .h .tplH

.tplH.h:
	$(M4) $(MACROS) $*.tplH > $*.h

.tplC.c:
	$(M4) $(MACROS) $*.tplC > $*.c

#.c.o:  ##should be implicit
#	$(CC) -c $(CFLAGS) $*.c

.tplC.o:
	$(M4) $(MACROS) $*.tplC > $*.c
	$(CC) -c $(CFLAGS) $*.c

