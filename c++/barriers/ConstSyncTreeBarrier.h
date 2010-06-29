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

#include <cstdio>

#ifndef BARRIER
  #define BARRIER     ConstTree::Barrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT ConstTree::Participant
#endif

#include <cstddef>
#include <pthread.h>
#include "../misc/misc.h"
#include "../misc/atomic.h"
#include "../misc/assert.h"
#include "../misc/lock.h"

namespace ConstTree {
  
  /**
   * Should be only set by compare-and-swap
   */
  union HelperNodeAtomicState {
    int value;
    struct bits {
      bool     winnerAlreadyReachedThisPoint : 1;
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
  
  class HelperNode : public TreeNode {
  public:    
    
    inline void initializeSynchronized(const bool sense) {
      flags.bits.winnerAlreadyReachedThisPoint = not sense;
    }
    
  private:
    HelperNodeAtomicState flags;  // actually here, version is never initialized, but that is not relevant to fulfill its purpose
    
    friend class Participant;
    friend class Barrier;
  };
  
  /**
   * Defines the properties which define the identity of a participant in the
   * tree, declared sparatly to be able to do stack allocation of Participants.
   */
  class ParticipantId {
    HelperNode* parent;
  };
  
  class Barrier;
  
  class Participant : public TreeNode {
  public:
    inline Participant(Barrier* const phaser);
    
    inline bool resume();
    
    inline void next() {
      awaitNextPhase();
      
      // now do the local sense reversal (global is already done on resume/synchronization)
      resumed = false;
      
      sense = !sense;
    }
    
    inline bool barrier() {
      bool result = resume();
      next();
      return result;
    }
    
    inline int getLeaveId()       const  {
    	assert(leaveId != 0);
    	return leaveId; }
    
    inline void setLeaveId(const int id)   { leaveId = id; }
    inline bool getSense()           const { return sense; }
    inline void notifyParticipants() const;
    
  private:
    
    //inline void doOptionalYield() const {
    //	if (DO_YIELD) {
    //		pthread_yield();
    //	}
    //}

    inline void awaitNextPhase() const {
      while ((*waitFlag) != sense)
      	/*doOptionalYield()*/ ;
    }
    
    bool resumed;
    int  leaveId; // leave ids start with 1, since this allows an easy recognition of the next insert node

    bool sense;
    volatile bool* waitFlag;
    
    Barrier* const phaser;
    
    friend class Barrier;
    friend class FreeParticipant;
    friend class TreeNode;
  };
  
  class Barrier {
  public:
    Barrier(const size_t)
    : lastleaveId(0),
      lastTreeNode(NULL),      
      evenIteration(true),
      sense(true),
      waitFlag(false)
    {
      assert(sizeof(HelperNodeAtomicState) == sizeof(int));
      lock_init(&insertAndDropLock, NULL);
    }
    
    inline void add(Participant* const participant) {
      // first make sure the participant is initalized correctly
      participant->sense         = sense;
      participant->waitFlag      = &waitFlag;
      
      lock_acquire(&insertAndDropLock);
      
      insertNewIntoTree(participant);
      
      lock_release(&insertAndDropLock);
    }    
    
    inline void finalize_initialization() {}
    
  private:
    
    inline void advanceFlags() {
      sense = !sense;
    }
    
    
    inline void insertNewIntoTree(Participant* const participant) {
      TreeNode* const insertNode = getNextInsertNode(lastTreeNode);
      
      participant->setLeaveId(getNextLeaveId());
      
      // if there is nothing yet, insert first node
      if (insertNode == NULL) {
        participant->setParent(NULL);
        // participant->insertLockIfInsertNode = &insertAndDropLock;
        lastTreeNode = participant;
        return;
      }
      
      // the general case, when there is already some tree
      
      
      HelperNode* const helper = new HelperNode();
      helper->setParent(insertNode->getParent());
            
      // use a strategy of over-announcement, i.e., insert the helper indicating
      // that the new node already synchronized. This will allow all other nodes
      // in the related subtree to announce synchronization consitently.
      // It does not pose problems since it will not imply final synchronization,
      // since the adding participant can not have synchronized yet.
      // There is no spawn-clocked/'spawn with phaser' after a resume(), which is a Good Thing(TM)
      helper->initializeSynchronized(participant->sense);
      
      // Not modify the tree
      // this could be a raise, but it is protected by the insertLock
      insertNode->setParent(helper);
      
      
      // the participant is the least important item, and can be handled now.
      // using stack-allocation, the thread exacuting this code here,
      // should allways be the participant anyway
      participant->setParent(helper);
      
      lastTreeNode = participant;
    }
    
    inline TreeNode* getNextInsertNode(Participant* const lastNode) const {
      if (!lastNode) {
        return NULL;
      }
      
      TreeNode* result = lastNode;
      
      int tmp = lastNode->getLeaveId();
      
      while ((tmp & 1) == 0) {  // while(tmp % 2 == 0)
      	//printf("getNextInsertNode: tmp=%d, result=%p\n", tmp, result);
      	assert(result != NULL);
        result = result->getParent();
        tmp >>= 1;            // tmp /= 2;
      }
      
      return result;
    }
    
    inline int getNextLeaveId() {
      //int old = lastleaveId;
      int result = atomic_add_and_fetch(&lastleaveId, 1);
      //printf("NextLeaveId: %d, old=%d, new=%d\n", result, old, lastleaveId);
      return result;
    }
    
    int                       lastleaveId; // leave id's start counting with 1
    Participant*     volatile lastTreeNode;
    lock_t insertAndDropLock;
    
    // global flags
    volatile bool evenIteration;
    volatile bool sense;
    volatile bool waitFlag;
    
    friend class Participant;
  };
  
  inline bool setResume(HelperNodeAtomicState* const flags,
                 const bool sense) {
    
    
    if (flags->bits.winnerAlreadyReachedThisPoint == sense)
      return true;
    
    flags->bits.winnerAlreadyReachedThisPoint = sense;
      
    return false;
  }
  
  /**
   * Propagate the resume up the tree.
   * Resume is always called on the leave node i.e. participant node first.
   *
   * REM we need to acquire the insert/drop lock if we are the insert node.
   *     The insert node with the look does not have to be the participant node,
   *     i.e. it might be one of the helper nodes up the tree
   */
  bool Participant::resume() {
    // do nothing if we already resumed, or don't signal at all
    if (resumed)
      return false;
    
    // ok, now we resumed this node
    resumed = true;
    
    // the hard part...
//#warning do we still need that lock???
    //lock_t* const lock = insertLockIfInsertNode;
    //if (lock) lock_acquire(lock);
    
    const bool sense         = getSense();

          HelperNode* node   = getParent();
        
    bool propagateResumeFurther   = true;
    HelperNodeAtomicState currentFlags;
    
    while (node && propagateResumeFurther) {
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        propagateResumeFurther = setResume(&newFlags,
                                           sense);
        
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
    }
    
    if (propagateResumeFurther) {
      notifyParticipants();
    }
    
    //if (lock) lock_release(lock);
    return propagateResumeFurther;      
  }
  
  Participant::Participant(Barrier* const phaser)
  : resumed(false), phaser(phaser) /*, insertLockIfInsertNode(NULL)*/ {
    phaser->add(this);
  }
    
  void Participant::notifyParticipants() const {
    phaser->advanceFlags();
    (*waitFlag) = sense;
  }
  
}
