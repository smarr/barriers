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

#include <cstddef>
#include <pthread.h>
#include "../misc/atomic.h"
#include "../misc/misc.h"

#ifndef BARRIER
  #define BARRIER SpinningCentralBarrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT Participant
#endif



class Participant;

class SpinningCentralBarrier {
public:
  
  SpinningCentralBarrier(int number_of_participants)
    : number_of_participants(number_of_participants),
      arrival_sense(false),
      arrived_participants(0) {}
  
  inline void finalize_initialization() {}
  
  bool do_barrier() {
    const bool sense   = arrival_sense;
    const int  arrived = atomic_add_and_fetch((int*)&arrived_participants, 1);
    
    if (arrived == number_of_participants) {
      arrived_participants = 0;
      arrival_sense = !arrival_sense;
      return true;
    }
    else {

      // spin until the barrier is completed
      while (sense == arrival_sense) {
        //if (DO_YIELD) {
        //  pthread_yield();
        //}
      }
      
      return false;
    }
  }

private:
  const    int  number_of_participants;
  
  volatile bool arrival_sense;
  volatile int  arrived_participants;

};

class Participant {
public:
  Participant(SpinningCentralBarrier* const barrier) : _barrier(barrier) {}
  
  inline bool resume() const {return false;}
  inline bool next() {
    return barrier();
  }
  
  inline bool barrier() const {
    return _barrier->do_barrier();
  }
private:
  SpinningCentralBarrier* const _barrier;
};
