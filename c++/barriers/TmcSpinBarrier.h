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

#include <tmc/spin.h>
#include <cstddef>

#ifndef BARRIER
#define BARRIER TmcSpinBarrier
#endif

#ifndef PARTICIPANT
#define PARTICIPANT Participant
#endif

class Participant;

class TmcSpinBarrier {
public:
  TmcSpinBarrier(const int numParticipants) : _barrier(TMC_SPIN_BARRIER_INIT(numParticipants)) {}
  
  inline bool do_barrier() {
    return 1 == tmc_spin_barrier_wait(&_barrier); // the first one to exit returns 1
  }
  
  void free() {}
  
  inline void finalize_initialization() {}
  
private:
  tmc_spin_barrier_t _barrier;
};

class Participant {
public:
  Participant(TmcSpinBarrier* const barrier) : _barrier(barrier) {}
  
  
  inline bool resume() const {return false;}
  inline bool next() const {
    return barrier();
  }
  
  inline bool barrier() const {
    return _barrier->do_barrier();
  }
private:
  TmcSpinBarrier* const _barrier;
};

