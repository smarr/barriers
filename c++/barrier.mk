# Basic configuration for all makefiles
# set up the required variables for all platforms and define the
# supported barriers
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


# optional config file
# that is stupid... I don't know how to include that config file correctly
# for the case that the main makefile is in another folder
-include ../../config.mk
-include ../../../config.mk
-include config.mk

# standard settings
SRC_DIR ?= $(shell pwd)
OPT     ?= -O3 -no_exceptions
NUM_PARTICIPANTS     ?= 2
SKIP_REFERENCE_TIME  ?= 0
DO_YIELD             ?= 0
OS ?= $(shell uname)

# required settings
LOG2_NUM_PARTICIPANTS := $(shell php -r 'echo ceil(log($(NUM_PARTICIPANTS), 2));')

DEFINES   = -DNUM_PARTICIPANTS=$(NUM_PARTICIPANTS) -DLOG2_NUM_PARTICIPANTS=$(LOG2_NUM_PARTICIPANTS) -DSKIP_REFERENCE_TIME=$(SKIP_REFERENCE_TIME) -DDO_YIELD=$(DO_YIELD)
LIBS     += -lpthread

CXXFLAGS += $(INCLUDES) $(DEFINES) -Wall $(OPT)
CFLAGS   += $(INCLUDES) $(DEFINES) -Wall $(OPT)
LFLAGS   += $(OPT)


# all portable barrier implementations, platfrom dependent ones are added later
BARRIERS = \
  DummyBarrier \
  SyncTreePhaser.full \
  ConstSpinningDisseminationBarrier \
  SpinningCentralBarrier \
  SpinningCentralCASBarrier \
  SpinningCentralDBarrier \
  SpinningDisseminationBarrier \
  SyncTreePhaser \
  ConstSyncTreeBarrier \
  HabaneroPhaser

DYNAMIC_BARRIERS = \
	SpinningCentralDBarrier \
	SyncTreePhaser \
	HabaneroPhaser \
	DummyBarrier


	#   SpinningTreeBarrier.b  is completely broken and superseeded by SyncTree*

ifeq "$(OS)" "Linux"
  BARRIERS += PthreadBarrier
endif

# make sure the PLATFROM is set
ifndef PLATFORM
  # fall back to Tilera if the MDE is available
	ifdef TILERA_ROOT
		PLATFORM = Tilera
	else
		PLATFORM = Intel
	endif
endif

# platform dependent settings
ifeq "$(PLATFORM)" "Tilera"
  LIBS += -ltmc
  CXX = tile-c++
  CC  = tile-cc
  BARRIERS += \
		TmcSpinBarrier \
		TmcSyncBarrier \
		TmcTokenBarrier
else

endif
