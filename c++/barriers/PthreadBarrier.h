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

#include <pthread.h>

#ifndef BARRIER
  #define BARRIER PthreadBarrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT Participant
#endif

class Participant;

class PthreadBarrier {
public:
  PthreadBarrier(const int numParticipants) {
    pthread_barrier_init(&_barrier, NULL, numParticipants);
  }
  
  inline bool do_barrier() {
    return PTHREAD_BARRIER_SERIAL_THREAD == pthread_barrier_wait(&_barrier);
  }

  void free() {
    pthread_barrier_destroy(&_barrier);
  }
  
  inline void finalize_initialization() {}
  
private:
  pthread_barrier_t _barrier;
};

class Participant {
public:
  Participant(PthreadBarrier* const barrier) : _barrier(barrier) {}
  
  inline bool resume() const {return false;}
  inline bool next() {
    return barrier();
  }
  
  inline bool barrier() const {
    return _barrier->do_barrier();
  }
private:
  PthreadBarrier* const _barrier;
};

