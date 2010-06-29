/*
 * Copyright (c) 2010 Stefan Marr, Vrije Universiteit Brussel
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

#include <stdio.h>
#include <pthread.h>

#ifndef DO_YIELD
  #define DO_YIELD true
#endif

#include "bench/epcc.h"
//#include "barriers/SpinningCentralBarrier.h"
//#include "barriers/SpinningCentralDBarrier.h"
//#include "barriers/SpinningDisseminationBarrier.h"
//#include "barriers/ConstSpinningDisseminationBarrier.h"
//#include "barriers/SpinningTreeBarrier.h"
//#include "barriers/ConstSyncTreeBarrier.h"
//#include "barriers/SyncTreePhaser.full.h"
#include "barriers/SyncTreePhaser.h"
//#include "barriers/HabaneroPhaser.h"
//#include "barriers/HabaneroHierarchicalPhaser.h"

#ifndef __APPLE__
  // Apple/OS X does not provide the barrier primitives
  //#include "barriers/PthreadBarrier.h"
#endif

/*
 These benchmarks are inspired by
 
 OpenMP MicroBenchmark Suite - Version 2.0
 
 And the article

 A Microbenchmark Suite for OpenMP 2.0
 J. Mark Bull and Darragh O'Neill SIGARCH Comput. Archit. News29(5):41--48(2001)
 (\cite{563656}, http://www.bibsonomy.org/bibtex/286b4f70669ea3e7fa8bc87424fed0218/gron)
 
 */


int main (int argc, const char * argv[]) {
#define NUM_PARTICIPANTS 2
#define LOG2_NUM_PARTICIPANTS 1
  EPCC<BARRIER, PARTICIPANT> epcc(2);
  
  //epcc.measureReferenceTime();
  epcc.measureBarrierPerformance();
  /*
  
  BARRIER     barrier(5);
  PARTICIPANT p1(&barrier);
  PARTICIPANT p2(&barrier);
  PARTICIPANT p3(&barrier);
  PARTICIPANT p4(&barrier);
  PARTICIPANT p5(&barrier);
  
  p1.resume();
  p2.resume();
  p3.resume();
  p4.resume();
  p5.resume();
  
  p5.next();
  p4.next();
  p2.next();
  p3.next();
  p1.next();
  
  p1.resume();
  p2.resume();
  p3.resume();
  p4.resume();
  p5.resume();
  
  p5.next();
  p4.next();
  p2.next();
  p3.next();
  p1.next();
  */
  pthread_exit(NULL);
  return 0;
}
