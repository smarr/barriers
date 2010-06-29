/*
 * Copyright (c) 2010 Stijn Verhaegen, Stefan Marr, Vrije Universiteit Brussel
 * <http://www.stefan-marr.de/>, <http://code.google.com/p/barriers/>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <pthread.h>

#include "absoh.h"

/*
 This is the main for the DYNPAR benchmark.
 It is parameterized by the build script with the definitions given below.
 */

#ifndef BARRIER
  //#include "../barriers/SpinningCentralDBarrier.h"
  #include "../barriers/SyncTreePhaser.h"

  //#error BARRIER was not defined. The build environment has to set it to the appropiate barrier class
#endif

#ifndef PARTICIPANT
  #error PARTICIPANT was not defined. The build environment has to set it to \
         the appropiate barrier participant class
#endif

#ifndef NUM_PARTICIPANTS
#define NUM_PARTICIPANTS 16
//  #error NUM_PARTICIPANTS has to be defined
#endif

#ifndef LOG2_NUM_PARTICIPANTS
  //#error LOG2_NUM_PARTICIPANTS has to be defined
#endif

#ifndef SKIP_REFERENCE_TIME
  #define SKIP_REFERENCE_TIME 1
#endif

int main (int argc, const char * argv[]) {
  ABSOH<BARRIER, PARTICIPANT> absoh(NUM_PARTICIPANTS);
  
  if (!SKIP_REFERENCE_TIME)
    absoh.measureReferenceTime();
  absoh.measureBarrierPerformance();
  
  pthread_exit(NULL);
  return 0;
}
