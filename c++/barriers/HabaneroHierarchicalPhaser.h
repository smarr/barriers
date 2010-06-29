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

#ifndef BARRIER
  #define BARRIER Habanero::Phaser
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT Habanero::Participant
#endif

#include "../misc/assert.h"
#include "../misc/atomic.h"
#include "../misc/lock.h"

#include <vector>
#include <limits.h>

using namespace std;


namespace Habanero {

  enum Mode { TRANSMIT, SIG, WAIT, SIG_WAIT, SINGLE };
  const static Mode defaultMode  = SIG_WAIT;
  const static int busyWaitCount = 100000;
  
  // For SIGNAL
  class SyncVar1 {
  public:
    SyncVar1(const int sigPhase, const int sigCycle,
             const Mode mode)
    : sigPhase(sigPhase), sigCycle(sigCycle),
      prevMWP(0), isResumed(false),
      isDropped(false), mode(mode) { }
    
    volatile int  sigPhase;
    int  sigCycle;
    int  prevMWP; // Previous mWaitPhase
    bool isResumed;
    volatile bool isDropped;
    Mode mode;
  };
  
  
  // For WAIT
  class SyncVar2 {
  public:
    volatile int waitPhase;
    int waitCycle;
    int prevMSP;
    bool isDropped;
    Mode mode;
    //#info we do not support single statements for the moment...
    //String loc;

    int mID;
    bool isMaster;
    
    SyncVar2(const int waitPhase, const int waitCycle, const int mID,
             const Mode mode)
    : waitPhase(waitPhase), waitCycle(waitCycle),
      prevMSP(0), isDropped(false),
      mode(mode), mID(mID), isMaster(false) { }
  };
  
  class Phaser;
  class SubPhaser;
  
  class Participant {
  public:
    Participant(Phaser* const phaser);
    
    void putSyncVar1(SyncVar1* const s1) { this->s1 = s1; }
    void putSyncVar2(SyncVar2* const s2) { this->s2 = s2; }
    
    SyncVar1* getSyncVar1() { return s1; }
    SyncVar2* getSyncVar2() { return s2; }
    
    inline bool resume();
    inline bool next();
    inline bool barrier();
    
    static void _resume(SyncVar1* const s1, SyncVar2* const s2) {
      assert(s1 != NULL && s2 != NULL); //	throw new PhaserException("Activity in " + mode + " cannot resume.");
      assert(!s1->isDropped && !s2->isDropped); // Dropped activity cannot resume.
      
      _signal(s1, s2);
    }
    
    static void _signal(SyncVar1* const s1, SyncVar2* const s2) {
      // Activity for non-SIGNAL does signal() -> No-op
      if (s1 == NULL)
        return;
      
      // Constraints for SIGNAL-WAIT ownership -> No-op
      if (s2 != NULL && s1->isResumed)
        return;
      
      assert(!s1->isDropped); // "Dropped activity cannot signal."
      
      s1->sigPhase++;
      s1->isResumed = true;
    }
    
  private:
    SubPhaser* const subPhaser;
    bool dropped;
    
    SyncVar1* s1;
    SyncVar2* s2;
  };

class SubPhaser : public Participant {
public:
  SubPhaser(Phaser* const phaser, SubPhaser* const parentPhaser)
  : initialParticipant(NULL), masterMode(TRANSMIT),
    swCounter(0), snglCounter(0),
    mWaitPhase(0), mSigPhase(0),
    masterID(0), minSigP(0),
    mSigCycle(0), mWaitCycle(0),
    sCastID(0), wCastID(0), minWaitP(0), parentPhaser(parentPhaser) { }

  
  void add(Participant* const participant) {
//#warning WARNING this is usually a data race
    if (initialParticipant == NULL) {
      initialParticipant = participant;
      _initialize(defaultMode);
    }
    _register(participant, defaultMode, initialParticipant);
  }
  

  
  void doWait(SyncVar1* const s1, SyncVar2* const s2) {
    
		// Activity for non-WAIT does wait() -> No-op
		if (s2 == NULL)
			return;
    
		assert(!s2->isDropped); // Dropped activity cannot wait.
      
    assert(s1 == NULL || s1->isResumed); // Signal is necessary before wait.
    
    s2->mID++;
		
    s2->isMaster = (s2->prevMSP <= s2->waitPhase && mSigPhase <= s2->waitPhase
                    && s2->mode == masterMode
                    && atomic_compare_and_swap_bool(&masterID, s2->mID - 1, s2->mID));
    
		//if (s2->mode == SINGLE) // do not support single ATM
		//	s2.loc = null;
    
		if (s1 != NULL)
			s1->isResumed = false;
    
		if (s2->isMaster) {
      _waitForWorkerSignals();
      
			if (s1 != NULL) {
        // mWaitPhase is always equal to minSigP for single/sig-wait mode
				mWaitPhase++;
        
				//if (s2->mode == SINGLE)  // unsupported ATM
				//	loc_of_master = s2.loc;
        
				mSigPhase++;
        
				if (!atomic_compare_and_swap_bool((int*)&sCastID, mSigPhase - 1, mSigPhase))
					; //_notifyAllWorkersOnS2(s2);
          
      }
      else {
        int t = INT_MAX;
        
        const int delta = (minSigP < t) ? minSigP - mWaitPhase : t - mWaitPhase;
            
        // Advance Mastar Sig/Wait Phase by delta (delta = 1 is the same
        // as usual);
        // 1. masterID += (delte-1) // masterID has already incremented
        // by one
        // 2. mWaitPhase += delta
        // 3. mSigPhase += delta
            
        if (delta > 1) {
          const bool flg = atomic_compare_and_swap_bool(&masterID, s2->mID, s2->mID + delta - 1);
          assert(flg);
        }
            
        // Signal to SIG activities when bounded phaser
        mWaitPhase += delta;
            
        // If a master blocks at point X, other wait-only activity may
        // reach here
        // as a new master. Next while-loop is to block such new master.
        while (sCastID < mSigPhase)
          ;
        
        mSigPhase += delta;
        
        // point X
        if (!atomic_compare_and_swap_bool((int*)&sCastID, mSigPhase - delta, mSigPhase)) {
          if (delta > 1) {
            const bool flg = atomic_compare_and_swap_bool((int*)&sCastID, mSigPhase
                                                      - delta + 1, mSigPhase);
            assert(flg);
          }
          //_notifyAllWorkersOnS2(s2);
        }
      }
      
		}
    else {
			if (s1 != NULL || s2->prevMSP <= s2->waitPhase)
				_waitForMasterSignal(s2);
        
        /*if (s2->mode == SINGLE && loc_of_master != null) Not supported ATM
          throw new PhaserException("Location of single statement doesn't match.");*/
    }
    
    // Signal to master when bounded phaser
    s2->waitPhase++;
  }
  
  inline void finalize_initialization() const {}
  
private:
  void _waitForWorkersInBound() {
		int first, size = 0;
		int min = INT_MAX;
    
		while (true) {
      lock_acquire(&lockForSyncVars2);
      	const int sz = syncVars2.size();
        if (sz == size) {
          lock_release(&lockForSyncVars2);
					break;
        }

        vector<SyncVar2*> lSyncVars2(syncVars2);
				first = size;
				size  = sz;
			lock_release(&lockForSyncVars2);
      
			for (int i = first; i < size; i++) {
				SyncVar2* const s2 = lSyncVars2.at(i);
        
				while (!s2->isDropped && s2->waitPhase <= mSigPhase)
					;
				if (!s2->isDropped) {
					const int s2WaitP = s2->waitPhase;
					if (s2WaitP < min)
						min = s2WaitP;
				}
			}
		}
    
		minWaitP = min;
	}
    
 /* void _notifyAllWorkersOnS2(SyncVar2* const s2) {
    lock_acquire(&lockForSyncVars2);
      vector<SyncVar2*> lSyncVars2(syncVars2);
    lock_release(&lockForSyncVars2);
    
		const int size = lSyncVars2.size();
		for (int i = 0; i < size; i++) {
			SyncVar2* const workS2 = lSyncVars2.at(i);
      
			if (workS2 != s2 && !workS2->isDropped) {
        atomic_add_and_fetch(&workS2->sync2Signal, 1);
        //lock_acquire(&workS2->lockForSSync2);
        //  pthread_cond_broadcast(&workS2->lockForSSync2Cond);
				//lock_release(&workS2->lockForSSync2);
			}
		}
	} */
  
  void _register(Participant* const participant,
                 const Mode mode,
                 Participant* const authorizer) {
    SyncVar1* const authSync1 = authorizer->getSyncVar1();
		SyncVar2* const authSync2 = authorizer->getSyncVar2();
    
		const bool availSignal = (authSync1 != NULL && !authSync1->isDropped);
		const bool availWait   = (authSync2 != NULL && !authSync2->isDropped);
    
		assert(availSignal || availWait);  //"TRANSMIT mode isn't permitted.")
    
		const Mode authMode = (availSignal) ? authSync1->mode : authSync2->mode;
    
		assert((mode == WAIT || availSignal) && (mode == SIG || availWait)
           && (mode != SINGLE || authMode == SINGLE)); //"Capability model violation: Parent mode = " + authMode + ", child mode = " + mode + ".");
    
		assert(!quiescent(authSync1)); // authorizer + "is not active on " + this + "; cannot transmit.");
    
		if (mode != WAIT)
			_register1(participant, authSync1->sigPhase, authSync1->sigCycle, mode);
		if (mode != SIG)
			_register2(participant, authSync2->waitPhase, authSync2->waitCycle,
                authSync2->mID, mode);
    
  }
  
  void _waitForMasterSignal(SyncVar2* const s2) {
		bool done = false;
    
		while (!done) {
			for (int j = 0; j < busyWaitCount; j++) {
				if (mSigPhase > s2->waitPhase) {
					done = true;
					s2->prevMSP = mSigPhase;
					break;
				}
			}
      
			if (!done) {
				while (sCastID < s2->waitPhase)
					;
        atomic_compare_and_swap((int*)&sCastID, s2->waitPhase, s2->waitPhase + 1);
        
				if (mSigPhase > s2->waitPhase) {
					done = true;
					s2->prevMSP = mSigPhase;
				}
			}
		}
	}
  
  inline bool quiescent(SyncVar1* const s1) const {
		return (s1 != NULL
            && !s1->isDropped
            && s1->isResumed
            && (s1->mode == SIG_WAIT || s1->mode == SINGLE));
	}
  
  void _register1(Participant* const p, const int sigPhase,
                  const int sigCycle, const Mode mode) {
		SyncVar1* const s1 = p->getSyncVar1();
    
		if (s1 != NULL && !s1->isDropped) {
			if (s1->mode == mode)
				return;
			else
				assert(false); // Multiple registration with different modes
      
		}
    else if (s1 != NULL) {
			s1->sigPhase = sigPhase;
			s1->sigCycle = sigCycle;
			s1->isResumed = false;
			s1->isDropped = false;
			s1->mode = mode;
		} 
    else {
			SyncVar1* const s1n = new SyncVar1(sigPhase, sigCycle, mode);
			p->putSyncVar1(s1n);
      
			lock_acquire(&lockForSyncVars1);
				syncVars1.push_back(s1n);
			lock_release(&lockForSyncVars1);
		}
	}
  
  void _register2(Participant* const p, const int waitPhase,
                  const int waitCycle, const int mID, const Mode mode) {
		SyncVar2* const s2 = p->getSyncVar2();
    
		if (s2 != NULL && !s2->isDropped) {
			if (s2->mode == mode)
				return;
			else
				assert(false); // Multiple registration with different modes
      
		}
    else if (s2 != NULL) {
			s2->waitPhase = waitPhase;
			s2->waitCycle = waitCycle;
			s2->isDropped = false;
			s2->mode = mode;
      
			s2->mID = mID;
			s2->isMaster = false;
      
			if      (mode == SIG_WAIT)
        atomic_add_and_fetch(&swCounter, 1);
			else if (mode == SINGLE)
				atomic_add_and_fetch(&snglCounter, 1);
      
		} 
    else {
			SyncVar2* const s2n = new SyncVar2(waitPhase, waitCycle, mID, mode);
      
			p->putSyncVar2(s2n);
      
			lock_acquire(&lockForSyncVars2);
				syncVars2.push_back(s2n);
			lock_release(&lockForSyncVars2);
			if (mode == SIG_WAIT)
				atomic_add_and_fetch(&swCounter, 1);
			else if (mode == SINGLE)
				atomic_add_and_fetch(&snglCounter, 1);
		}
	}
  
  
  
  void _initialize(Mode mode) {
    assert(mode != TRANSMIT);
    //assert(bound >= 0); //"Bounded size must be greater or equal to 0."
        
		if (mode != WAIT) {
			SyncVar1* const s1 = new SyncVar1(0, 0, mode);
			initialParticipant->putSyncVar1(s1);
			syncVars1.push_back(s1);
		}
    
		if (mode != SIG) {
			SyncVar2* const s2 = new SyncVar2(0, 0, 0, mode);
			initialParticipant->putSyncVar2(s2);
			syncVars2.push_back(s2);
			if (mode == SIG_WAIT) {
        atomic_add_and_fetch(&swCounter, 1);
      }
			else if (mode == SINGLE) {
        atomic_add_and_fetch(&snglCounter, 1);
      }
			masterMode = mode;
		}
    
    lock_init(&lockForSyncVars1, NULL);
    lock_init(&lockForSyncVars2, NULL);
  }
  
  void _waitForWorkerSignals() {
		int first, size = 0;
		int min = INT_MAX;
    
		while (true) {

      lock_acquire(&lockForSyncVars1);
        const int sz = syncVars1.size();
        if (sz == size) {
          lock_release(&lockForSyncVars1);
          break;
        }
			
        vector<SyncVar1*> lSyncVars1(syncVars1);
				
        first = size;
				size = sz;
      lock_release(&lockForSyncVars1);
      
			for (int i = first; i < size; i++) {
				SyncVar1* const s1 = lSyncVars1.at(i);
        
				while (!s1->isDropped && s1->sigPhase <= mWaitPhase)
					;
				if (!s1->isDropped) {
					int s1SigP = s1->sigPhase;
					if (s1SigP < min)
						min = s1SigP;
				}
			}
		}
    
		minSigP = min;
	}
  
  Participant* volatile initialParticipant;
  Mode masterMode;
  int swCounter;
  int snglCounter;
  
  volatile int mWaitPhase;
  volatile int mSigPhase;
  
  int masterID;
  
  int minSigP;
  
  int mSigCycle;
  int mWaitCycle;
  
  volatile int sCastID;
  volatile int wCastID;
  
  int minWaitP;
  
  vector<SyncVar1*> syncVars1;
  vector<SyncVar2*> syncVars2;
  
  lock_t lockForSyncVars1;
  lock_t lockForSyncVars2;
  
  SubPhaser* const parentPhaser;
};
  
  class Phaser {
  public:
    Phaser(const int numTiers, const int degree)
    : numTiers(numTiers), degree(degree), registeredParticipants(0) {}
    
    Phaser(const size_t)
    : numTiers(6), degree(2), registeredParticipants(0) {} // standard values choosen to have a binary tree for 64 participants, not actually for best performance...
    
    
  private:
    void _initialize() {
      // we put the tiers in the first dimension, and the degrees in the 2nd
      // at least, thats how I would conclude it from the HP paper
      // but why don't they use just a simple array to store the tree?
      // probably to avoid explicit index caluclations? but this here seems to be even harder
      subPhasers.resize(numTiers);
      for (int t = 0; t < numTiers; t++) {
        vector<SubPhaser*>* tier = new vector<SubPhaser*>((t + 1) * degree);
        subPhasers.at(0) = tier;
        
        //for (int d = 0; <#condition#>; <#increment#>) {
        //  <#statements#>
        //}
      }
    }
    
    vector<vector<SubPhaser*>*> subPhasers;
    
    const int numTiers;
    const int degree;
    int registeredParticipants;
  };
  
  Participant::Participant(Phaser* const phaser) : /*phaser(phaser),*/ subPhaser(NULL), dropped(false), s1(NULL), s2(NULL) {
    phaser->add(this);
  }

  bool Participant::resume() {
    _resume(getSyncVar1(), getSyncVar2());
    return false;
  }

  bool Participant::next() {
    phaser->doWait(getSyncVar1(), getSyncVar2());
    return false;
  }
  
  
  bool Participant::barrier() {
    phaser->resume(getSyncVar1(), getSyncVar2());
    phaser->doWait(getSyncVar1(), getSyncVar2());
    return false;
  }

}

