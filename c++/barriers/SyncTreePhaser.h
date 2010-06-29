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

//#define YIELD 0 
//290

#ifndef BARRIER
  #define BARRIER     SyncTree::Phaser
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT SyncTree::Participant
#endif

#include <cstddef>
#include <cstdlib>
#include "../misc/atomic.h"
#include "../misc/assert.h"
#include "../misc/misc.h"
#include "../misc/lock.h"


//+#define USE_ALIGNMENT 1
//+#include <arch/chip.h>
//+#include <malloc.h>

namespace SyncTree {
  
  enum Mode {
    SIGNAL_WAIT_NEXT,
		SIGNAL_WAIT,
		SIGNAL_ONLY,
		WAIT_ONLY
  };
  
#define IsLeft(helper, child) (helper->leftChild == child)
#define PHASE_BITS 15
#define MAX_PHASE  0x7FFF
#define HALF_PHASE 0x3FFF
#define TRUNCATED_PHASE(phase) ((phase) & MAX_PHASE)
  
  inline bool LT(const unsigned int a, const unsigned int b) {
    if (abs(a - b) > HALF_PHASE) {
      return (b < HALF_PHASE);
    }
    return a < b;
  }
  
  inline bool GT_or_EQ(const unsigned int a, const unsigned int b) {
    if (abs(a - b) > HALF_PHASE) {
      return b >= HALF_PHASE;
    }
    return a >= b;
  }
  
  /**
   * Should be only set by compare-and-swap
   */
  union HelperNodeAtomicState {
    volatile int value;
    struct bits {
      bool waitOnlyL:  1;  // this can also mean, that the participant has been dropped, 
                                    // the consequence is, that the participant will not do any resumes
      bool waitOnlyR:  1;
      unsigned int phaseLeft : PHASE_BITS;  // very small, we are using a heuristic to detect overflows
                                            // phases should not be appart for more then half of MAX_PHASE
      unsigned int phaseRight: PHASE_BITS;
    } bits;
  };
  
  class HelperNode;
  class Participant;
  
  class TreeNode {
  public:
    inline TreeNode() : parent(NULL) {}
    
    inline HelperNode* getParent() const       { return parent; }
    inline void setParent(HelperNode* const p) { parent = p; }

  private:
    HelperNode* volatile parent;
  };
  
  class HelperNode : public TreeNode {
  public:
    HelperNode() : TreeNode(), leftChild(NULL) {
      flags.value = 0;
    }

    inline void initialize(const unsigned int phase, const TreeNode* const leftChild) {
      flags.bits.waitOnlyL  = false;
      flags.bits.waitOnlyR  = false;
      flags.bits.phaseLeft  = phase;
      flags.bits.phaseRight = phase;
      this->leftChild = leftChild;
    }
    
#ifdef USE_ALIGNMENT
    void* operator new(size_t size) {
      return memalign(CHIP_L2_LINE_SIZE(), size);
    }
#endif
    
    inline bool isLeftChild(const Participant* const p) {
      return (const TreeNode*)p == leftChild;
    }
    
  private:
    HelperNodeAtomicState flags;  // actually here, version is never initialized, but that is not relevant to fulfill its purpose
    
    const TreeNode* leftChild;    // this is only necessary to know which phase counter to advance, its not really possible to
                                  // know which opponent it is from the leave id, at least not if trees are not complerely full
    
    friend class Participant;
    friend class Phaser;
  };
  
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
    }
    
    inline bool barrier() {
      bool result = resume();
      next();
      return result;
    }
    
    inline Mode getMode() const { return mode; }
    inline void notifyParticipants() const;
    
  private:
    
    inline void doAllAddActions(const bool reusedNode);
    inline bool doAllDropActions();
    
    //inline void doOptionalYield() const {
    //	if (DO_YIELD) {
    //		pthread_yield();
    //	}
    //}

    inline void awaitNextPhase() const {
      //volatile int i;
      //volatile float a = 0.0;
      while (LT(*globalPhase, phase)) {
      //  for (i = 0; i < YIELD; i++) a += i;
      //	doOptionalYield();
      }
    }
    
  public:
    const Mode mode;
    bool resumed;
    unsigned int  phase;   // indicates the phase for this participant

    volatile unsigned int* globalPhase; // point to the phasers phase, think it safes only one indirection, but well
    
    Phaser* const phaser;
    
    friend class Phaser;
    friend class FreeParticipant;
    friend class TreeNode;
  };
  
  class FreeParticipant {
    inline FreeParticipant(const Participant* const participant)
    : next(NULL), isLeft(participant->getParent()->isLeftChild(participant)), parent(participant->getParent()) {}
  private:
    FreeParticipant* next; // next item in the queue
    
    const bool isLeft;
    HelperNode* const parent;
    
    friend class Phaser;
  };
  
  class Phaser {
  public:
    inline Phaser(const size_t)
    : leaveCount(0),
      lastTreeNode(NULL), 
      firstFree(NULL),
      lastFree(NULL),
      phase(0)
    {
      assert(sizeof(HelperNodeAtomicState) == sizeof(int));
      lock_init(&insertAndDropLock, NULL);
    }
    
    inline void add(Participant* const participant) {
      // first make sure the participant is initalized correctly
      participant->globalPhase = &phase;
      
      lock_acquire(&insertAndDropLock);
      
      if (firstFree) {
        reuseFree(participant);
      }
      else {
        insertNewIntoTree(participant);
      }
      
      lock_release(&insertAndDropLock);
    }    
    
    inline void finalize_initialization() const {}
    
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
      //participant->setLeaveId(freeP->leaveId);
      
      participant->doAllAddActions(true);
    }
    
    inline bool drop(Participant* const participant) {
      bool result = false;
      
      lock_acquire(&insertAndDropLock);
      HelperNode* const parent = participant->getParent();
      
      if (parent) {
        result = participant->doAllDropActions();
        
        if (participant == lastTreeNode) {
          // assert(lastTreeNode->insertLockIfInsertNode);
          // make sure we do not have any dangling pointers
          lastTreeNode = NULL;
        }
        
        // now create the new FreeParticipant and enqueue it
        FreeParticipant* const freeP = new FreeParticipant(participant);
        
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
      
      return result;
    }
    
    inline void insertNewIntoTree(Participant* const participant) {
      // just checking, that we have not missed any free nodes
      assert(firstFree == NULL);
      assert(lastFree  == NULL);
      
      TreeNode* const insertNode = getNextInsertNode(lastTreeNode);
      
      // increase leave count, this determines next insert position
      leaveCount++;
      //participant->setLeaveId(leaveCount);
      
      // if there is nothing yet, insert first node
      if (insertNode == NULL) {
        participant->setParent(NULL);
        lastTreeNode = participant;
        return;
      }
      
      // the general case, when there is already some tree
      const unsigned int phase = participant->phase;
      
      HelperNode* const helper = new HelperNode();
      HelperNode* const insertParent = insertNode->getParent();
      helper->setParent(insertParent);
      
      helper->initialize(phase, insertNode);
      
      // make sure that the waitOnly is set, getting this info is a bit
      // difficulte here, insertNode is the last leave, or it is a helper
      // this does not need to be atomic since WAIT_ONLY flags only change
      // protected by the insert lock we already acquired
      if (   (insertNode == lastTreeNode && (lastTreeNode->getMode() == WAIT_ONLY))
          || (   ((HelperNode*)insertNode)->flags.bits.waitOnlyL
              && ((HelperNode*)insertNode)->flags.bits.waitOnlyR))
      {
        assert(not helper->flags.bits.waitOnlyL); // just make sure it was correctly initialized...
        helper->flags.bits.waitOnlyL = true;
      }
      
      if (participant->mode == WAIT_ONLY) {
        helper->flags.bits.waitOnlyR = true;
      }
      
      // ensure that the initialization is in the memory, before the insert node could see its new parent
      memory_fence();
      
      // Now modify the tree
      insertNode->setParent(helper);
      
      // make sure this change is propergated properly
      memory_fence();
      
      // the racing insert node will already update the helper node now
      // and we can take the phase value from the parent and set it
      
      // First do an atomic update on the insertParent, and remove the over signalization
      unsigned int numSignals = 0;
      if (insertParent) {
        bool cas_successful = false;
        HelperNodeAtomicState currentValue = insertParent->flags;
        do {
          HelperNodeAtomicState newValue = currentValue;

          // remember the signals we have to set on the helper
          numSignals = currentValue.bits.phaseRight - phase;
          assert(numSignals >= 0);  // well, it should be at least in the current phase, otherwise something is going entierly wrong with the whole tree
          
          newValue.bits.phaseRight = phase;
          newValue.bits.waitOnlyR  = newValue.bits.waitOnlyR && (participant->mode == WAIT_ONLY);
          
          // now the new flags are prepared, and we can try to install them
          HelperNodeAtomicState oldFlags;
          oldFlags.value = atomic_compare_and_swap((int*)&insertParent->flags.value, currentValue.value, newValue.value);
          
          if (oldFlags.value == currentValue.value) {
            // CAS was sucessful, and we can exit the loop
            cas_successful = true;
          }
          else {
            // lets try again, but use reuse the read value for next round
            currentValue = oldFlags;
          }
        } while (not cas_successful);
      }
      
      // Second make sure that the signals are set on the helper if necessary
      if (numSignals != 0) {
        bool keep_trying = true;
        HelperNodeAtomicState currentValue = helper->flags;
        do {
          if (currentValue.bits.phaseLeft != phase) {
            break;
          }
          
          HelperNodeAtomicState newValue = currentValue;
          
          // set the signals from the insertParent here
          newValue.bits.phaseLeft += numSignals;
          
          // now the new flags are prepared, and we can try to install them
          HelperNodeAtomicState oldFlags;
          oldFlags.value = atomic_compare_and_swap((int*)&helper->flags.value, currentValue.value, newValue.value);
          
          if (oldFlags.value == currentValue.value) {
            // CAS was sucessful, and we can exit the loop
            keep_trying = false;
          }
          else {
            if (oldFlags.bits.phaseLeft == phase) {
              keep_trying = true;
            }
            else {
              currentValue = oldFlags;
              keep_trying = false;
            }
          }
        } while (keep_trying);
      }
      
      
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
  public:
    volatile int              leaveCount;
    Participant*     volatile lastTreeNode;
    FreeParticipant* volatile firstFree;
    FreeParticipant* volatile lastFree;
    lock_t insertAndDropLock;
    
    // global phase, used as waitFlag
    volatile unsigned int  phase;
    
    friend class Participant;
  };
  
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
    if (mode != SIGNAL_ONLY) {
      resumed = true;
    }
    
    // the hard part...
    phase = TRUNCATED_PHASE(phase + 1);

    HelperNode* node   = getParent();
    const TreeNode* lastNode = this;
        
    bool propagateResumeFurther = true;
    HelperNodeAtomicState currentFlags;
    
    while (node && propagateResumeFurther) {
      const bool isLeft = IsLeft(node, lastNode);
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      // propagateResumeFurther does not need to be CASSafe, we do not read it
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;

        if (isLeft) {
          newFlags.bits.phaseLeft  = phase;
          propagateResumeFurther = 
                  currentFlags.bits.waitOnlyR     // if they are wait only, we will lose 
                  // if the opponent already set the phase to ours, or bigger, we also have lost
                  || GT_or_EQ(currentFlags.bits.phaseRight, phase);
        }
        else {
          newFlags.bits.phaseRight = phase;
          propagateResumeFurther = 
                  currentFlags.bits.waitOnlyL    // if they are wait only, we will lose 
                  // if the opponent already set the phase to ours, or bigger, we also have lost
                  || GT_or_EQ(currentFlags.bits.phaseLeft,  phase);
        }

        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap((int*)&node->flags.value, currentFlags.value, newFlags.value);
        
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
    
    if (propagateResumeFurther) {
      notifyParticipants();
    }
    
    //if (lock) pthread_mutex_unlock(lock);
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
  inline bool Participant::doAllDropActions() {
    const Mode mode          = this->mode;
        HelperNode* node     = getParent();
    const TreeNode* lastNode = this;
    //int nodeId = leaveId;
    
    if (mode == WAIT_ONLY) {
      // in this case there is nothing changed,
      // WAIT_ONLY indicated instant resume,
      // and this is exatly what we need for dropped node
      return false;
    }
    
    // otherwise, we need to indicate a resume, so that we are proceeding to
    // the next phase
    phase = TRUNCATED_PHASE(phase + 1);
    const unsigned int myPhase = phase;
    
    bool propagateResumeFurther   = true;
    bool propagateWaitOnlyFurther = true;
    HelperNodeAtomicState currentFlags;
    
    while (node && (propagateWaitOnlyFurther || propagateResumeFurther)) {
      const bool isLeft = IsLeft(node, lastNode);
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      bool CASSafe_propagateResumeFurther   = propagateResumeFurther;
      bool CASSafe_propagateWaitOnlyFurther = propagateWaitOnlyFurther;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        // the resume has always to be done first, to be able to
        // have the right propagateResumeFurther value,
        // which is based on what any other participant saw earlier
        
        if (propagateResumeFurther) {
          if (isLeft) {
            newFlags.bits.phaseLeft = phase;
            
            CASSafe_propagateResumeFurther =
                currentFlags.bits.waitOnlyR    // if they are wait only, we will lose 
                // if the opponent already set the phase to ours, or bigger, we also have lost
                || GT_or_EQ(currentFlags.bits.phaseRight, myPhase);
          }
          else {
            newFlags.bits.phaseRight = phase;

            CASSafe_propagateResumeFurther =
                currentFlags.bits.waitOnlyL    // if they are wait only, we will lose 
                // if the opponent already set the phase to ours, or bigger, we also have lost
                || GT_or_EQ(currentFlags.bits.phaseLeft,  myPhase);
          }
        }
        
        if (propagateWaitOnlyFurther) {
          if (isLeft) {
            newFlags.bits.waitOnlyL = true;
          }
          else {
            newFlags.bits.waitOnlyR = true;
          }
            
          // if BOTH participants have said that they are waitOnly, we need to propagate that
          CASSafe_propagateWaitOnlyFurther = newFlags.bits.waitOnlyL && newFlags.bits.waitOnlyR;
        }
        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap((int*)&node->flags.value, currentFlags.value, newFlags.value);
        
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
      
      propagateResumeFurther   = CASSafe_propagateResumeFurther;
      propagateWaitOnlyFurther = CASSafe_propagateWaitOnlyFurther;
           
      lastNode = node;
      node     = node->getParent();
      //nodeId >>= 1;
    }
    
    if (propagateResumeFurther) {
      notifyParticipants();
    }
    
    return propagateResumeFurther;
  }
    
  /**
   * When a node is added we need to do the following:
   *  - if it is a reused node and WAIT_ONLY, nothing changes and we return
   *  - if it is not reused, we have to set the WAIT_ONLY flags and possibly
   *    resume, this is equivalent to doing all drop actions
   *  Standard case:
   *  - if its mode is not WAIT_ONLY, we need to revoke the waitOnly flags
   *  - need to set all phases on our path to the minimum i.e. the current global phase
   *    value, which is the same as with what the added participant starts
   *
   * For that we travers up the tree, until we can stop (find a node where no 
   * signalisation was done), or are at the root.
   *
   * IMPORTANT: here we have a data race, if the other subtree already announces it synchronization
   *            but then we reset the phase of our subtree (which would have indicated the the racing subtree lost and has to travers up)
   *            The problem is, when we are actually faster to reach the parent and to set the phase,
   *            That would be ignored(not seen) by the racing subtree and we would have oversynchronization.
   *            Thus, we have to make sure, that the events are ordered, by waiting until this node has the values we would expect from the last
   *            level.
   *
   * ASSERT already acquired insertLock, this is a add action,
   *        it should only be called in that context and the lock
   *        has to be acquired
   */
  inline void Participant::doAllAddActions(const bool reusedNode) {
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
    HelperNode* node   = getParent();
    const TreeNode* lastNode = this;
    
    if (not reusedNode) {
      // if this is a new node, then its parent phaseRight == phase
      // by definition/initialization, the same is true for its parent
      // and well, we dont have to bother with revoking wait-only of course (cf. few lines below)
      
      // this is a small optimization, but also allows to avoid special handling of this case in the code below
      if (node) { // == insertNode.parent
        assert(node->flags.bits.phaseRight == phase);
        lastNode = node;
        node = node->getParent();
        
        if (node) { // == insertNode.parent.parent (still not interesting, thus going to its parent)
          assert(node->flags.bits.phaseRight == phase);
          lastNode = node;
          node = node->getParent();
        }
      }
    }
    
    // ok, now let's revoke all WAIT_ONLY flags and resumes
    // count the revoked resumes
    bool setMinPhaseFurther    = true;
    bool revokeWaitOnlyFurther = true;
    HelperNodeAtomicState currentFlags;
    
    bool alreadyTouchedSomeNode = false;
    unsigned int  expectedPhaseForNextLevel = 0;
    
    while (node && (setMinPhaseFurther || revokeWaitOnlyFurther)) {
      const bool isLeftNode = IsLeft(node, lastNode);
      bool compareAndSwap_failed = true;
      currentFlags.value = node->flags.value;
      
      // here we want to ensure that all races with subtree
      // of the last node we touched are imposible
      if (alreadyTouchedSomeNode) {
        bool foundExpectedValue = false;
        
        do {
          if (isLeftNode) {
            foundExpectedValue = currentFlags.bits.phaseLeft  == expectedPhaseForNextLevel;
          }
          else {
            foundExpectedValue = currentFlags.bits.phaseRight == expectedPhaseForNextLevel;
          }

          
          if (not foundExpectedValue)
            currentFlags.value = node->flags.value;
        }
        while (not foundExpectedValue);
      }
      
      // ok, now we are sure, that all racing activities which have encountered
      // the old, false value for our subtree's phase have written their stuff
      // and we can reset it the the subtree's minimum

      bool CASSafeResult_setMinPhaseFurther    = setMinPhaseFurther;
      bool CASSafeResult_revokeWaitOnlyFurther = revokeWaitOnlyFurther;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        if (setMinPhaseFurther) {
          if (isLeftNode) {
            CASSafeResult_setMinPhaseFurther = newFlags.bits.waitOnlyR || GT_or_EQ(newFlags.bits.phaseRight, phase);            
            newFlags.bits.phaseLeft  = phase;
          }
          else {
            CASSafeResult_setMinPhaseFurther = newFlags.bits.waitOnlyL || GT_or_EQ(newFlags.bits.phaseLeft,  phase);
            newFlags.bits.phaseRight = phase;
          }
        }

        
        if (revokeWaitOnlyFurther) {
          
          CASSafeResult_revokeWaitOnlyFurther = newFlags.bits.waitOnlyL &&  newFlags.bits.waitOnlyL;  // go up iff both opponents are waitOnly
          if (isLeftNode) {
            //assert(newFlags.bits.waitOnlyL); // otherwise it should have never been propagated up the tree
            newFlags.bits.waitOnlyL = false;
          }
          else {
            //assert(newFlags.bits.waitOnlyR); // otherwise it should have never been propagated up the tree
            newFlags.bits.waitOnlyR = false;
          }
        }

        
        // now the new flags are prepared, and we can try to install them
        HelperNodeAtomicState oldFlags;
        oldFlags.value = atomic_compare_and_swap((int*)&node->flags.value, currentFlags.value, newFlags.value);
        
        if (oldFlags.value == currentFlags.value) {
          // CAS was sucessful, and we can exit the loop
          compareAndSwap_failed = false;
          
          if (isLeftNode) {
            expectedPhaseForNextLevel = newFlags.bits.phaseRight; // thats what a racing opponent should also set in the next level
          }
          else {
            expectedPhaseForNextLevel = newFlags.bits.phaseLeft;  // thats what a racing opponent should also set in the next level
          }
        }
        else {
          // lets try again, but use reuse the read value for next round
          currentFlags = oldFlags;
        }
      }
      while (compareAndSwap_failed);
      
      
      setMinPhaseFurther    = CASSafeResult_setMinPhaseFurther;
      revokeWaitOnlyFurther = CASSafeResult_revokeWaitOnlyFurther;
      
      lastNode = node;
      node = node->getParent();
    }
  }
  
  Participant::Participant(Phaser* const phaser)
  : mode(SIGNAL_WAIT), resumed(false), phase(phaser->phase), phaser(phaser) {
    phaser->add(this);
  }
  
  Participant::Participant(Phaser* const phaser, const Mode mode)
  : mode(mode), resumed(false), phase(phaser->phase), phaser(phaser) {
    phaser->add(this);
  }
  
  void Participant::notifyParticipants() const {
    phaser->phase = TRUNCATED_PHASE(phaser->phase + 1);
  }
  
  bool Participant::drop() {
    return phaser->drop(this);
  }
  
}
