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
 *
 * Implements the dissemination barrier algorithm presented in \cite{103729}
 * Mellor-Crummey, John M. & Scott, Michael L.: 
 *  Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors
 *  In: ACM Trans. Comput. Syst. , Vol. 9 , Nr. 1 New York, NY, USA: ACM (1991).
 */

#include <cmath>
#include <cstddef>
#include <cstdio>
#include "../misc/atomic.h"

#ifndef BARRIER
  #define BARRIER SpinningDisseminationBarrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT SpinningDisseminationBarrier::Participant
#endif

class SpinningDisseminationBarrier {
public:
  
  class Participant {
  public:
    Participant(SpinningDisseminationBarrier* const barrier)
     : flags(new volatile bool*[2]), partner(new volatile bool**[2]),
       parity(0), sense(true), isMaster(false)
    {
      barrier->add(this);
    }
    
    void initialize(const size_t rounds) {
      this->rounds = rounds;
      flags[0] = new bool[rounds];
      flags[1] = new bool[rounds];
      
      for (short parity = 0; parity < 2; parity++) {
        for (size_t round = 0; round < rounds; round++) {
          flags[parity][round] = false;
          //printf("&flags[%zu][%zu] = %p\n", parity, round, &flags[parity][round]);
        }
      }
      
      partner[0] = new volatile bool*[rounds];
      partner[1] = new volatile bool*[rounds];
    }
    
    inline bool resume() const {return false;}
    inline bool next() {
      return barrier();
    }
    
    inline bool barrier() {
      for (size_t dissemRound = 0; dissemRound < rounds; dissemRound++) {			
        *partner[parity][dissemRound] = sense;
        
        
        volatile bool* flag = &flags[parity][dissemRound];
        while (*flag != sense);
      }
      
      if (parity == 1) {
        sense = !sense;
      }
      
      parity = 1 - parity;
      
      // chose the master task statically
      return isMaster;
    }
    
    void becomeMaster() {
      isMaster = true;
    }
    
    void free() {
      delete[] flags[0];
      delete[] flags[1];
      delete[] flags;
      
      delete[] partner[0];
      delete[] partner[1];
      delete[] partner;
    }
    
    volatile bool** const flags;
    volatile bool*** const partner;
    
    short parity;
    bool  sense;
    size_t rounds;
  private:
    bool isMaster;
  };
  
  
  SpinningDisseminationBarrier(const size_t number_of_participants)
  : number_of_participants(number_of_participants),
    rounds((size_t)ceil(log2(number_of_participants))),
    next_id(-1),
    participants(new Participant*[number_of_participants])
  {
  

  }
  
  void finalize_initialization() {
    // first make sure all participants are initialized correctly (memory needs to be allocated)
    for (size_t i = 0; i < number_of_participants; i++) {
      participants[i]->initialize(rounds);
    }
    
    
    // now, we can assign partners
    for (size_t i = 0; i < number_of_participants; i++) {     
      for (size_t dissemRounds = 0; dissemRounds < rounds; dissemRounds++) {
        int partnerId = (i + (1 << dissemRounds)) % number_of_participants; //(i + 2^k) mod P
        participants[i]->partner[0][dissemRounds] = &participants[partnerId]->flags[0][dissemRounds]; 
        participants[i]->partner[1][dissemRounds] = &participants[partnerId]->flags[1][dissemRounds];
      }
    }
  }
  

  
  void free() {
    for (size_t round = 0; round < rounds; round++) {
      participants[round]->free();
    }
    
    delete[] participants;
  }
  
  void add(Participant* const participant) {
    const int id = atomic_add_and_fetch(&next_id, 1);
    
    if (id == 0) {
      participant->becomeMaster();
    }
    
    participants[id] = participant;
  }
  
  /*Participant* getBarrierParticipant(size_t id) {
    return &participants[id];
  }*/
  
private:
  const size_t number_of_participants;
  const size_t rounds;
  
  int next_id;

  Participant** participants;
    
};

