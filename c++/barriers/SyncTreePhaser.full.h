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

#define YIELD 0
//0

#include <cstdio>

#ifndef BARRIER
  #define BARRIER     SyncTreeSimplified::Phaser
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT SyncTreeSimplified::Participant
#endif

#include <cstddef>
#include <pthread.h>
#include "../misc/atomic.h"
#include "../misc/assert.h"
#include "../misc/misc.h"
#include "../misc/lock.h"


//+#define USE_ALIGNMENT 1
#ifdef __TILECC__
  #include <arch/chip.h>
  #include <malloc.h>
#endif

namespace SyncTreeSimplified {
  
  enum Mode {
    SIGNAL_WAIT_NEXT,
		SIGNAL_WAIT,
		SIGNAL_ONLY,
		WAIT_ONLY
  };
  
  union EvenOddBits {
    int combinedValue;
    struct bits {
      volatile bool even : 1;
      volatile bool odd  : 1;
    } bits;
  };
  
  /**
   * Should be only set by compare-and-swap
   */
  union HelperNodeAtomicState {
    int value;
    struct bits {
      int      version                           : 16;
      unsigned int waitOnly                      : 2;  // this can also mean, that the participant has been dropped, the result is, that this participant will not do any resumes
      bool     winnerAlreadyReachedThisPointEven : 1;  // I reintroduced the even/odd flags since we are no going to do down propagation in the tree, and if a node which already got the signal resumes in the next phase and waits on a node which has not got yet the signal from the last iteration, it sees a wrongly signaled node, this is avoided with this idea of parity/dual-flags
      bool     winnerAlreadyReachedThisPointOdd  : 1;
    } bits;
  };
  
  class HelperNode;
  class Participant;
  
  class TreeNode {
  public:
    inline TreeNode() : parent(NULL) {}
    
    inline HelperNode* getParent() const { return parent; }
    inline void setParent(HelperNode* const p) { parent = p; }

  private:
    HelperNode* volatile parent;
  };

#ifdef __TILECC__  
  #define newBool (bool*)memalign(CHIP_L2_LINE_SIZE(), sizeof(bool))
#else
  #define newBool new bool()
#endif
  
  class HelperNode : public TreeNode {
  public:
    HelperNode()
    : waitFlagEven(newBool), waitFlagOdd(newBool),
      leftChildEven(NULL), leftChildOdd(NULL),
      rightChildEven(NULL), rightChildOdd(NULL)
    {
      flags.value = 0;
    }
    /**
     * The idea is that we set the flag for the current iteration to signaled i.e. sense
     * The flag for the next iteration should be set to not signaled.
     * REM: on even iterations we revert sense
     */
    inline void initializeSynchronized(const bool evenIteration, const bool sense) {
      this->evenIteration = evenIteration;
      
      if (evenIteration) {
        flags.bits.winnerAlreadyReachedThisPointEven = sense;
        // next iteration is odd, and does not change sense, thus,
        // to be not signaled, we need 'not sense'
        flags.bits.winnerAlreadyReachedThisPointOdd  = not sense;
        
        // the wait flags have to be initialized un-signaled
        *waitFlagEven = not sense;
        *waitFlagOdd  = not sense; // since in the next iteration, sense does not change
      }
      else {
        flags.bits.winnerAlreadyReachedThisPointOdd  = sense;
        // on the transition to the next iteration, we change sense
        // thus, a not signaled state means setting now to 'sense'
        flags.bits.winnerAlreadyReachedThisPointEven = sense;
        
        // the wait flags have to be initialized un-signaled
        *waitFlagOdd  = not sense;
        *waitFlagEven = sense; // next iteration sense will change, thus, it will be un-signaled this way
      }
    }
    
#ifdef USE_ALIGNMENT
    void* operator new(size_t size) {
      return memalign(CHIP_L2_LINE_SIZE(), size);
    }
#endif
    
  private:
    HelperNodeAtomicState flags;  // actually here, version is never initialized, but that is not relevant to fulfill its purpose
    
    bool evenIteration;
    volatile bool *const  waitFlagEven;
    volatile bool *const  waitFlagOdd;
    volatile bool *volatile leftChildEven;
    volatile bool *volatile leftChildOdd;
    volatile bool *volatile rightChildEven;
    volatile bool *volatile rightChildOdd;
    
    friend class Participant;
    friend class Phaser;
  };
  
  /**
   * Defines the properties which define the identity of a participant in the
   * tree, declared sparatly to be able to do stack allocation of Participants.
   */
  //class ParticipantId {
    HelperNode* parent;
    //Opponent    treePosition;
  //};
  
  class Phaser;
  
  class Participant : public TreeNode {
  public:
    inline Participant(Phaser* const phaser);
    inline Participant(Phaser* const phaser, const Mode mode);
    
    inline ~Participant() {
      drop();
    }
    
#ifdef USE_ALIGNMENT
    void* operator new(size_t size) {
      return memalign(CHIP_L2_LINE_SIZE(), size);
    }
#endif
    
    inline bool drop();
    
    inline bool resume();
    
    inline void next() {
      if (mode != SIGNAL_ONLY) {
        awaitNextPhase();
      }
      
      // now do the local sense reversal (global is already done on resume/synchronization)
      resumed = false;
      
      evenIteration = !evenIteration;
      if (evenIteration) {
        sense = !sense;
      }      
    }
    
    inline bool barrier() {
      bool result = resume();
      next();
      return result;
    }
    
//    inline int getLeaveId()       const  {
//    	assert(leaveId != 0);
//    	return leaveId; }
//    inline void setLeaveId(const int id) {   leaveId = id; }
    
    inline bool isEvenIteration() const { return evenIteration; }
    inline bool getSense()        const { return sense;         }
    inline Mode getMode()         const { return mode;          }
    
    inline void notifyParticipants(HelperNode* const lastNode) const;
    
  private:
    
    inline void doAllAddActions(const bool reusedNode) const;
    inline bool doAllDropActions() const;
    
    inline void doOptionalYield() const {
    	if (DO_YIELD) {
    		pthread_yield();
    	}
    }

    inline void awaitNextPhase() /*const*/ {
      HelperNode* const node = waitNode;
      
      if (evenIteration) {
	volatile int i;
	volatile float a = 0.0;
        while ((*node->waitFlagEven) != sense) {
	  for (i = 0; i < YIELD; i++) a += i;
          doOptionalYield();
        }
        
        volatile bool* const l = node->leftChildEven;
        if (l) { *l  = sense; }
        
        volatile bool* const r = node->rightChildEven;
        if (r) { *r = sense;  }
      }
      else {
	volatile int i;
	volatile float a = 0.0;
        while ((*node->waitFlagOdd)  != sense) {
	  for (i = 0; i < YIELD; i++) a += i;
          doOptionalYield();
        }
        
        volatile bool* const l = node->leftChildOdd;
        if (l) { *l  = sense; }
        
        volatile bool* const r = node->rightChildOdd;
        if (r) { *r = sense;  }        
      }
    }
    
    
    const Mode mode;
    bool resumed;
    int  leaveId; // leave ids start with 1, since this allows an easy recognition of the next insert node

    bool evenIteration;
    bool sense;
    HelperNode*  waitNode;
    
    Phaser* const phaser;
    
    //lock_t* volatile insertLockIfInsertNode;
    
    friend class Phaser;
    friend class FreeParticipant;
    friend class TreeNode;
  };
  
  class FreeParticipant {
    inline FreeParticipant(const Participant* const participant)
    : next(NULL), leaveId(participant->leaveId), parent(participant->getParent()) {}
  private:
    FreeParticipant* next; // next item in the queue
    
    const int leaveId;
    HelperNode* const parent;
    
    friend class Phaser;
  };
  
  class Phaser {
  public:
    Phaser(const size_t)
    : leaveCount(0),
      lastTreeNode(NULL), 
      firstFree(NULL),
      lastFree(NULL),
      
      evenIteration(true),
      sense(true)
    {
      assert(sizeof(HelperNodeAtomicState) == sizeof(int));
      lock_init(&insertAndDropLock, NULL);
    }
    
    inline void add(Participant* const participant) {
      // first make sure the participant is initalized correctly
      participant->sense         = sense;
      participant->evenIteration = evenIteration;
      
      lock_acquire(&insertAndDropLock);
      
      if (firstFree) {
        reuseFree(participant);
      }
      else {
        insertNewIntoTree(participant);
      }
      
      lock_release(&insertAndDropLock);
    }    
    
    inline void finalize_initialization() {}
    
  private:
    inline void reuseFree(Participant* const participant) {     
      FreeParticipant* const freeP = firstFree;
      
      firstFree = freeP->next;
      if (firstFree == NULL) {
        lastFree = NULL;
      }
      
      if (lastTreeNode == NULL) {
        // that should be fine, insertNode is only zero, if we dropped the node where insertNode was pointing at
        //participant->insertLockIfInsertNode = &insertAndDropLock;
        lastTreeNode = participant;
      }
      
      participant->setParent(freeP->parent);
//      participant->setLeaveId(freeP->leaveId);
      
      participant->doAllAddActions(true);
    }
    
    inline void drop(Participant* const participant) {
      lock_acquire(&insertAndDropLock);
      HelperNode* const parent = participant->getParent();
      
      if (parent) {
        participant->doAllDropActions();
        
        if (participant == lastTreeNode) {
          // assert(lastTreeNode->insertLockIfInsertNode);
          // make sure we do not have any dangling pointers
          lastTreeNode = NULL;
        }
        
        // now create the new FreeParticipant and enqueue it
        FreeParticipant* const  freeP = new FreeParticipant(participant);
        
        if (lastFree) {
          lastFree->next = freeP;
        }
        
        lastFree = freeP;
        
        // this is the case if the queue is empty
        if (!firstFree) {
          assert(freeP->next == NULL);
          firstFree = freeP;
        }
      }
      else {
        // it was the only participant, lets just set all relevant pointers to NULL
        leaveCount = 0;
        lastTreeNode = NULL;
        assert(firstFree == NULL);
      }
      
      lock_release(&insertAndDropLock);
    }
    
    inline void advanceFlags() {
      evenIteration = !evenIteration;
      if (evenIteration) {
        sense = !sense;
      }
    }
    
    inline void insertNewIntoTree(Participant* const participant) {
      // just checking, that we have not missed any free nodes
      assert(firstFree == NULL);
      assert(lastFree  == NULL);
      
      TreeNode* const insertNode = getNextInsertNode(lastTreeNode);
      
      // increase leave count, this determines next insert position
      leaveCount++;
      
      // if there is nothing yet, insert first node
      if (insertNode == NULL) {
        participant->setParent(NULL);
        // participant->insertLockIfInsertNode = &insertAndDropLock;
        lastTreeNode = participant;
        return;
      }
      
      // the general case, when there is already some tree
      
      
      HelperNode* const helper = new HelperNode();
      HelperNode* const insertParent = insertNode->getParent();
      helper->setParent(insertParent);
      
      // set the wait flags
      if (insertParent) {
        insertParent->rightChildEven = helper->waitFlagEven;
        insertParent->rightChildOdd  = helper->waitFlagOdd;
      }
      
      if (insertNode != lastTreeNode) {
        helper->leftChildEven = ((HelperNode*)insertNode)->waitFlagEven;
        helper->leftChildOdd  = ((HelperNode*)insertNode)->waitFlagOdd;
      }
      
      // make sure that the waitOnly is set, getting this info is a bit
      // difficulte here, insertNode is the last leave, or it is a helper
      // this does not need to be atomic since WAIT_ONLY flags only change
      // protected by the insert lock we already acquired
      if (   (insertNode == lastTreeNode && (lastTreeNode->getMode() == WAIT_ONLY))
          || ((HelperNode*)insertNode)->flags.bits.waitOnly == 2) {
        assert(helper->flags.bits.waitOnly == 0); // just make sure it was correctly initialized...
        helper->flags.bits.waitOnly++;
      }
      
      // use a strategy of over-announcement, i.e., insert the helper indicating
      // that the new node already synchronized. This will allow all other nodes
      // in the related subtree to announce synchronization consitently.
      // It does not pose problems since it will not imply final synchronization,
      // since the adding participant can not have synchronized yet.
      // There is no spawn-clocked/'spawn with phaser' after a resume(), which is a Good Thing(TM)
      helper->initializeSynchronized(participant->isEvenIteration(), participant->getSense());
      
      // Not modify the tree
      // this could be a raise, but it is protected by the insertLock
      insertNode->setParent(helper);
      
      
      // the participant is the least important item, and can be handled now.
      // using stack-allocation, the thread exacuting this code here,
      // should allways be the participant anyway
      participant->setParent(helper);
      
      participant->doAllAddActions(false);
      
      lastTreeNode = participant;
    }
    
    inline TreeNode* getNextInsertNode(Participant* const lastNode) const {
      if (!lastNode) {
        return NULL;
      }
      
      TreeNode* result = lastNode;
      
      int tmp = leaveCount;
      
      while ((tmp & 1) == 0) {  // while(tmp % 2 == 0)
      	//printf("getNextInsertNode: tmp=%d, result=%p\n", tmp, result);
      	assert(result != NULL);
        result = result->getParent();
        tmp >>= 1;            // tmp /= 2;
      }
      
      return result;
    }
        
    volatile int              leaveCount;
    Participant*     volatile lastTreeNode;
    FreeParticipant* volatile firstFree;
    FreeParticipant* volatile lastFree;
    lock_t  insertAndDropLock;
    
    // global flags
    volatile bool evenIteration;
    volatile bool sense;
    
    friend class Participant;
  };
  
  /**
   * Returns whether we have lost here
   */
  inline bool setResume(HelperNodeAtomicState* const flags,
                 const bool evenIteration,
                 const bool sense) {
    if (evenIteration) {
      if (flags->bits.winnerAlreadyReachedThisPointEven == sense)
        return true; // we lost
      
      flags->bits.winnerAlreadyReachedThisPointEven = sense;
        
      // if the opponent wount do any signaling, we cannot win here
      return flags->bits.waitOnly > 0;
    }
    else {
      if (flags->bits.winnerAlreadyReachedThisPointOdd == sense)
        return true; // we lost
      
      flags->bits.winnerAlreadyReachedThisPointOdd = sense;
      
      // if the opponent wount do any signaling, we cannot win here
      return flags->bits.waitOnly > 0;
    }
  }
  
  inline bool propagateWaitOnly(HelperNodeAtomicState* const flags) {
    /*if (flags->bits.isWaitOnly != NONE) {
      // that is the old value, and only our direct opponent should have had the chance to drop this node
      assert(   (opponent == LEFT  && flags->bits.isWaitOnly == RIGHT)
             || (opponent == RIGHT && flags->bits.isWaitOnly == LEFT));
    }*/
    
    flags->bits.waitOnly++;
    
    // if BOTH participants have said that they are waitOnly, we need to propagate that
    return flags->bits.waitOnly == 2;
  }
  
  /**
   * Propagate the resume up the tree.
   * Resume is always called on the leave node i.e. participant node first.
   *
   * REM we need to acquire the insert/drop lock if we are the insert node.
   *     The insert node with the look does not have to be the participant node,
   *     i.e. it might be one of the helper nodes up the tree
   */
  inline bool Participant::resume() {
    // do nothing if we already resumed, or don't signal at all
    if (resumed || mode == WAIT_ONLY) {
      return false;
    }
    
    // ok, now we resumed this node
    resumed = true;
    
    // the hard part...
//#warning do we still need that lock???
    //lock_t* const lock = insertLockIfInsertNode;
    //if (lock) lock_acquire(lock);
    
    const bool sense         = getSense();
    const bool evenIteration = isEvenIteration();
          HelperNode* node   = getParent();
          HelperNode* lastNode = NULL;
        
    bool propagateResumeFurther   = true;
    HelperNodeAtomicState currentFlags;
    
    while (node && propagateResumeFurther) {
      //assert(!lastNode || (node->leftChild == &lastNode->waitFlag || node->rightChild == &lastNode->waitFlag));
      
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        propagateResumeFurther = setResume(&newFlags,
                                           evenIteration,
                                           sense);

        newFlags.bits.version += 1;  // use the version to make sure
                                     // that we never missed an update, that is important for
                                     // reads done concurrently with a revoke done for readding dropped nodes
        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap(&node->flags.value, currentFlags.value, newFlags.value);
        
        if (oldFlags.value == currentFlags.value) {
          // CAS was sucessful, and we can exit the loop
          compareAndSwap_failed = false;
        }
        else {
          // lets try again, but use reuse the read value for next round
          currentFlags = oldFlags;
        }
      }
      while (compareAndSwap_failed);
      
      lastNode = node;
      node = node->getParent();
    }
    
    waitNode = lastNode; // we have won here, thats were we will wait
    
    assert(waitNode);
    
    if (propagateResumeFurther) {
      notifyParticipants(lastNode);
    }
    
    //if (lock) lock_release(lock);
    return propagateResumeFurther;      
  }
  
  /**
   * When a node is dropped we need to do the following:
   *  - propagete the drop up the tree, 
   *    this is done by propagating that the node is WAIT_ONLY
   *  - do the resume, includes the notify!
   *
   * ASSERT already acquired insertLock, this is a drop action,
   *        it should only be called in that context and the lock
   *        has to be acquired
   */
  inline bool Participant::doAllDropActions() const {
    const bool sense         = getSense();
    const Mode mode          = this->mode;
          HelperNode* node   = getParent();
          HelperNode* lastNode = NULL;
    
    if (mode == WAIT_ONLY) {
      // in this case there is nothing changed,
      // WAIT_ONLY indicated instant resume,
      // and this is exatly what we need for dropped node
      return false;
    }
    
    bool propagateResumeFurther   = true;
    bool propagateWaitOnlyFurther = true;
    HelperNodeAtomicState currentFlags;
    
    while (node && (propagateWaitOnlyFurther || propagateResumeFurther)) {
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        // the resume has always to be done first, to be able to
        // have the right propagateResumeFurther value,
        // which is based on what any other participant saw earlier
        
        if (propagateResumeFurther) {
          propagateResumeFurther   = setResume(&newFlags,
                                               evenIteration,
                                               sense);
        }
        
        if (propagateWaitOnlyFurther) {
          propagateWaitOnlyFurther = propagateWaitOnly(&newFlags);
        }
        
        newFlags.bits.version += 1;
        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap(&node->flags.value, currentFlags.value, newFlags.value);
        
        if (oldFlags.value == currentFlags.value) {
          // CAS was sucessful, and we can exit the loop
          compareAndSwap_failed = false;
        }
        else {
          // lets try again, but use reuse the read value for next round
          currentFlags = oldFlags;
        }
      }
      while (compareAndSwap_failed);
      
      lastNode = node;
      node     = node->getParent();
    }
    
    if (propagateResumeFurther) {
      notifyParticipants(lastNode);
    }
    
    return propagateResumeFurther;
  }
  
  /**
   * @return bool whether it was a real win
   */
  inline bool reSetResume(HelperNodeAtomicState* const flags,
                   const bool evenIteration,
                   const bool sense) {
    if (evenIteration) {
      if (flags->bits.winnerAlreadyReachedThisPointEven == sense)
        return false;
      
      flags->bits.winnerAlreadyReachedThisPointEven = sense;
      
      // check whether we could win here
      const bool couldWin = flags->bits.waitOnly == 0;
      return couldWin;
    }
    else {
      if (flags->bits.winnerAlreadyReachedThisPointOdd == sense)
        return false;
      
      flags->bits.winnerAlreadyReachedThisPointOdd = sense;
      
      // check whether we could win here
      const bool couldWin = flags->bits.waitOnly == 0;
      return couldWin;
    }
  }
  
  /**
   * When a node is added we need to do the following:
   *  - if it is a reused node and WAIT_ONLY, nothing changes and we return
   *  - if it is not reused, we have to set the WAIT_ONLY flags and possibly
   *    resume, this is equivalent to doing all drop actions
   *  Standard case:
   *  - if its mode is not WAIT_ONLY, we need to revoke the waitOnly flags
   *  - need to revoke resumes which result from instant resume (also those
   *    which stem from the fact that there was no node before)
   *
   * To do the revoke, we proceed as follows:
   *   We need to travers up two times.
   *   First time, we remove all WAIT_ONLY flags and all resumes,
   *   while counting the resumes. We stop as soon as resume was not announced.
   *   
   *   We avoide problems with concurrent participants by making sure, that
   *   every participant increments a version counter on its visit.
   *   With that, we know that the read data is reliable and that we do not
   *   see a flag and go on while it actually has been revoked in between.
   *
   * ASSERT already acquired insertLock, this is a add action,
   *        it should only be called in that context and the lock
   *        has to be acquired
   */
  inline void Participant::doAllAddActions(const bool reusedNode) const {
    if (mode == WAIT_ONLY) {
      if (reusedNode) {
        // in this case there is nothing changed,
        // WAIT_ONLY indicated instant resume,
        // and this is exatly what we had for the dropped node
        return;
      }
      else {
        // it is a WAIT_ONLY node, so, what we have to do here, is basically
        // the same that we have to do on a drop
        doAllDropActions();
        return;
      }
    }
    
    // well, now we know that it is not a WAIT_ONLY node, so we have to do
    // the stuff for the standard case
    const bool sense         = getSense();
    const bool evenIteration = isEvenIteration();
          HelperNode* node   = getParent();
    
    // ok, now let's revoke all WAIT_ONLY flags and resumes
    // count the revoked resumes
    bool revokeResumeFurther   = true;
    bool revokeWaitOnlyFurther = reusedNode;
    HelperNodeAtomicState currentFlags;
    
    // count the winning resumes, i.e., don't count the resumes where
    // was not actually won
    int revokedResumes = 0;
    
    while (node && (revokeResumeFurther || revokeWaitOnlyFurther)) {
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      int tmpRevokedResumes;
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        tmpRevokedResumes = revokedResumes; // need this since CAS could fail
        
        if (revokeResumeFurther) {
          if (evenIteration) {
            if (newFlags.bits.winnerAlreadyReachedThisPointEven == sense) {
              newFlags.bits.winnerAlreadyReachedThisPointEven = not sense;
              // and of course, count it
              tmpRevokedResumes++;
              revokeResumeFurther = true;
            }
            else {
              // if the opponent wount do any signaling, no-one could have won here
              // and thus, we can not count it
              revokeResumeFurther = newFlags.bits.waitOnly > 0;
            }
          }
          else {
            if (newFlags.bits.winnerAlreadyReachedThisPointOdd == sense) {
              newFlags.bits.winnerAlreadyReachedThisPointOdd = not sense;
              // and of course, count it
              tmpRevokedResumes++;
              revokeResumeFurther = true;
            }
            else {
              // if the opponent wount do any signaling, no-one could have won here
              // and thus, we can not count it
              revokeResumeFurther = newFlags.bits.waitOnly > 0;
            }
          }
        }
        
        if (revokeWaitOnlyFurther) {
          assert(newFlags.bits.waitOnly > 0); // otherwise it should have never been propagated up the tree
          
          revokeWaitOnlyFurther = newFlags.bits.waitOnly == 2;  // go up iff both opponents are waitOnly
          newFlags.bits.waitOnly--;
        }

        newFlags.bits.version += 1;
        
        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap(&node->flags.value, currentFlags.value, newFlags.value);
        
        if (oldFlags.value == currentFlags.value) {
          // CAS was sucessful, and we can exit the loop
          compareAndSwap_failed = false;
        }
        else {
          // lets try again, but use reuse the read value for next round
          currentFlags = oldFlags;
        }
      }
      while (compareAndSwap_failed);
    
      revokedResumes = tmpRevokedResumes;
      
      node = node->getParent();
    }
    
    // now, the second part of the game
    // reset the resume flags
    node = getParent();
    
    revokedResumes -= 1; // we need exactly one resume less, since we added a un-resumed node
    
    while (revokedResumes > 0) {
      assert(node);
      
      currentFlags = node->flags;
      
      bool compareAndSwap_failed = true;
      bool didWin;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        didWin = reSetResume(&newFlags, evenIteration, sense);
        
        newFlags.bits.version += 1;
        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap(&node->flags.value, currentFlags.value, newFlags.value);
        
        if (oldFlags.value == currentFlags.value) {
          // CAS was sucessful, and we can exit the loop
          compareAndSwap_failed = false;
        }
        else {
          // lets try again, but use reuse the read value for next round
          currentFlags = oldFlags;
        }
      }
      while (compareAndSwap_failed);
      
      node = node->getParent();
      
      if (didWin) {
        revokedResumes -= 1;
      }
    }
  }
  
  Participant::Participant(Phaser* const phaser)
  : mode(SIGNAL_WAIT), resumed(false), waitNode(NULL), phaser(phaser) {
    phaser->add(this);
  }
  
  Participant::Participant(Phaser* const phaser, const Mode mode)
  : mode(mode), resumed(false), waitNode(NULL), phaser(phaser) {
    phaser->add(this);
  }
  
  void Participant::notifyParticipants(HelperNode* const lastNode) const {
    phaser->advanceFlags();
    memory_fence();
    if (evenIteration) {
      *lastNode->waitFlagEven = sense;
    }
    else {
      *lastNode->waitFlagOdd  = sense;
    }
  }
  
  bool Participant::drop() {
    // first resume
    bool result = resume();
    
    // then modify the tree
    phaser->drop(this);
    
    return result;
  }
  
}
