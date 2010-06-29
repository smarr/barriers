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

typedef void *(*pthread_routine)(void*);

template <class BarrierClass, typename ParticipantType>
class LIOH {
public:
  
  LIOH(const int numParticipants)
    : outerDelay(100000), //100ms
			delayI(10000), //10ms
			delayS(500),  //0.5ms
      innerReps(100),
      numParticipants(numParticipants),
      initalization_finished(false),
      barrier(new BarrierClass(numParticipants))
  {
    printPreamble();
  }
  
  void measureReferenceTime();
  void measureBarrierPerformance();
  
  static void delay(int delayLength);
  static void delayTime(unsigned int delta);
  
  const size_t outerDelay;
  const size_t delayI;
  const size_t delayS;
  const size_t innerReps;
  const size_t numParticipants;
  
  BarrierClass* getBarrier() { return barrier; }
  
  volatile bool initalization_finished;
  
private:
  void printPreamble();
  void calculateAndPrintStatistics(double *mtp, double *sdp);
  
  void spawnThreads();
  
  BarrierClass* const barrier;
};

// we need the implementation in the header
#include "lioh.impl.h"

