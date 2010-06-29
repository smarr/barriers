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
 *
 * Implements the dissemination barrier algorithm presented in \cite{103729}
 * Mellor-Crummey, John M. & Scott, Michael L.: 
 *  Algorithms for Scalable Synchronization on Shared-Memory Multiprocessors
 *  In: ACM Trans. Comput. Syst. , Vol. 9 , Nr. 1 New York, NY, USA: ACM (1991).
 */

#include <cstddef>
#include <cmath>
#include "../misc/atomic.h"

#ifndef BARRIER
  #define BARRIER ConstSpinningDisseminationBarrier<NUM_PARTICIPANTS, LOG2_NUM_PARTICIPANTS>
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT ConstSpinningDisseminationBarrier<NUM_PARTICIPANTS, LOG2_NUM_PARTICIPANTS>::Participant
#endif

template<int NumParticipants, int Rounds>
class ConstSpinningDisseminationBarrier {
public:
  
  class Participant {
  public:
    Participant(ConstSpinningDisseminationBarrier* barrier): parity(0), sense(true), isMaster(false) {
      barrier->add(this);
      initialize();
    }
    
    inline bool resume() const {return false;}
    inline bool next() {
      return barrier();
    }
    
    inline bool barrier() {
      for (size_t dissemRound = 0; dissemRound < Rounds; dissemRound++) {			
        *partner[parity][dissemRound] = sense;
        
        
        volatile bool* const flag = &flags[parity][dissemRound];
        while (*flag != sense);
      }
      
      if (parity == 1) {
        sense = !sense;
      }
      
      parity = 1 - parity;
      
      // chose the master task statically
      return isMaster;
    }
    
    void free() {}
  
    volatile bool flags[2][Rounds];
    volatile bool* partner[2][Rounds];
    
    int  parity;
    bool sense;
    
    void becomeMaster() {
      isMaster = true;
    }
    
  private:
    void initialize() {      
      for (size_t parity = 0; parity < 2; parity++) {
        for (size_t round = 0; round < Rounds; round++) {
          flags[parity][round] = false;
        }
      }
    }
    
    bool isMaster;
  };
  
  
  ConstSpinningDisseminationBarrier(const size_t) : next_id(-1) {}
  
  /**
   * Ensure that all invariants are fulfilled.
   */
  void finalize_initialization() {
    // assign partners
    for (size_t i = 0; i < NumParticipants; i++) {     
      for (int dissemRounds = 0; dissemRounds < Rounds; dissemRounds++) {
        int partnerId = (i + (1 << dissemRounds)) % NumParticipants; //(i + 2^k) mod P
        participants[i]->partner[0][dissemRounds] = &participants[partnerId]->flags[0][dissemRounds]; 
        participants[i]->partner[1][dissemRounds] = &participants[partnerId]->flags[1][dissemRounds];
      }
    }
  }
  
  void free() {
    for (size_t round = 0; round < Rounds; round++) {
      participants[round].free();
    }
    
    delete[] participants;
  }
    
private:
  void add(Participant* const participant) {
    const int id = atomic_add_and_fetch(&next_id, 1);
    
    if (id == 0) {
      participant->becomeMaster();
    }
    
    participants[id] = participant;
  }
  
  int next_id;

  Participant* participants[NumParticipants];
    
};
