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

#include <stddef.h>
#include <pthread.h>
#include "../misc/atomic.h"
#include "../misc/misc.h"

#ifndef BARRIER
  #define BARRIER SpinningCentralDBarrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT Participant
#endif



class Participant;

class SpinningCentralDBarrier {
public:
  
  SpinningCentralDBarrier(const int)
    : number_of_participants(0),
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
//        if (DO_YIELD) {
//         pthread_yield();
//        }
      }
      
      return false;
    }
  }

private:
  inline void _register() {
    atomic_add_and_fetch((int*)&number_of_participants,  1);
  }
  
  inline void _drop() {
    atomic_add_and_fetch((int*)&number_of_participants, -1);
  }
  
  volatile int  number_of_participants;
  
  volatile bool arrival_sense;
  volatile int  arrived_participants;

  friend class Participant;
};

class Participant {
public:
  Participant(SpinningCentralDBarrier* const barrier) : _barrier(barrier), dropped(false) {
    _barrier->_register();
  }
  
  ~Participant() {
    drop();
  }
  
  inline bool resume() const {return false;}
  
  inline bool next() {
    return barrier();
  }
  
  inline void drop() {
    if (not dropped) {
      // WARNING: do you see the race here? But I dont care, just use it correctly!!!
      dropped = true;
      _barrier->_drop();
    }
  }
  
  inline bool barrier() const {
    return _barrier->do_barrier();
  }
  
private:
  SpinningCentralDBarrier* const _barrier;
  volatile bool dropped;
};
