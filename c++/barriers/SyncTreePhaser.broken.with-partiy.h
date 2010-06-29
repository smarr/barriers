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
  #define BARRIER     SyncTree::Phaser
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT SyncTree::Participant
#endif

#include <cstddef>
#include <pthread.h>
#include "../misc/atomic.h"
#include "../misc/assert.h"
#include "../misc/misc.h"

namespace SyncTree {
  
  enum Mode {
    SIGNAL_WAIT_NEXT,
		SIGNAL_WAIT,
		SIGNAL_ONLY,
		WAIT_ONLY
  };
  
  enum Opponent {
    NONE  = 0x00,
    LEFT  = 0x01,
    RIGHT = 0x02,
    BOTH  = 0x03 // could be extended to have an n-ary tree
  };
  
#define OPPONENT_FROM_leaveId(leaveId) ((leaveId & 1) ? LEFT : RIGHT)
  
  /**
   * Should be only set by compare-and-swap
   */
  union HelperNodeAtomicState {
    int value;
    struct bits {
      int      version                           : 16;
      Opponent isWaitOnly                        : 2;  // this can also mean, that the participant has been dropped, the result is, that this participant will not do any resumes
      bool     winnerAlreadyReachedThisPointEven : 1;
      bool     winnerAlreadyReachedThisPointOdd  : 1;
    } bits;
  };
  
  class HelperNode;
  class Participant;
  
  class TreeNode {
  public:
    TreeNode() : parent(NULL) {}
    
    inline HelperNode* getParent() const { return parent; }
    inline void setParent(HelperNode* const p) { parent = p; }

  private:
    HelperNode* volatile parent;
  };
  
  class HelperNode : public TreeNode {
  public:    
    /**
     * The idea is that we set the flag for the current iteration to signaled i.e. sense
     * The flag for the next iteration should be set to not signaled.
     * REM: on even iterations we revert sense
     */
    inline void initializeSynchronized(const bool evenIteration, const bool sense) {
      if (evenIteration) {
        flags.bits.winnerAlreadyReachedThisPointEven = sense;
        // next iteration is odd, and does not change sense, thus,
        // to be not signaled, we need 'not sense'
        flags.bits.winnerAlreadyReachedThisPointOdd  = not sense;
      }
      else {
        flags.bits.winnerAlreadyReachedThisPointOdd  = sense;
        // on the transition to the next iteration, we change sense
        // thus, a not signaled state means setting now to 'sense'
        flags.bits.winnerAlreadyReachedThisPointEven = sense;
      }
    }
    
  private:
    HelperNodeAtomicState flags;  // actually here, version is never initialized, but that is not relevant to fulfill its purpose
    
    friend class Participant;
    friend class Phaser;
  };
  
  /**
   * Defines the properties which define the identity of a participant in the
   * tree, declared sparatly to be able to do stack allocation of Participants.
   */
  class ParticipantId {
    HelperNode* parent;
    Opponent    treePosition;
  };
  
  class Phaser;
  
  class Participant : public TreeNode {
  public:
    inline Participant(Phaser* const phaser);
    inline Participant(Phaser* const phaser, const Mode mode);
    
    inline ~Participant() {
      drop();
    }
    
    
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
    
    inline int getLeaveId()       const  {
    	assert(leaveId != 0);
    	return leaveId; }
    inline void setLeaveId(const int id) {   leaveId = id; }
    
    inline bool isEvenIteration() const { return evenIteration; }
    inline bool getSense()        const { return sense;         }
    inline Mode getMode()         const { return mode;          }
    
    inline void notifyParticipants() const;
    
  private:
    
    inline void doAllAddActions(const bool reusedNode) const;
    inline bool doAllDropActions() const;
    
    inline void doOptionalYield() const {
    	if (DO_YIELD) {
    		pthread_yield();
    	}
    }

    inline void awaitNextPhase() const {
      if (evenIteration) {
        while ((*waitFlagEven) != sense)
        	doOptionalYield();
      }
      else {
        while ((*waitFlagOdd)  != sense)
        	doOptionalYield();
      }
    }
    
    
    const Mode mode;
    bool resumed;
    int  leaveId; // leave ids start with 1, since this allows an easy recognition of the next insert node

    bool evenIteration;
    bool sense;
    volatile bool* waitFlagEven;
    volatile bool* waitFlagOdd;
    
    Phaser* const phaser;
    
    //pthread_mutex_t* volatile insertLockIfInsertNode;
    
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
    inline Phaser(const size_t)
    : lastleaveId(0),
      lastTreeNode(NULL), 
      firstFree(NULL),
      lastFree(NULL),
      
      evenIteration(true),
      sense(true),
      waitFlagEven(false),
      waitFlagOdd(false)
    {
      assert(sizeof(HelperNodeAtomicState) == sizeof(int));
      pthread_mutex_init(&insertAndDropLock, NULL);
    }
    
    inline void add(Participant* const participant) {
      // first make sure the participant is initalized correctly
      participant->sense         = sense;
      participant->evenIteration = evenIteration;
      participant->waitFlagEven  = &waitFlagEven;
      participant->waitFlagOdd   = &waitFlagOdd;
      
      pthread_mutex_lock(&insertAndDropLock);
      
      if (firstFree) {
        reuseFree(participant);
      }
      else {
        insertNewIntoTree(participant);
      }
      
      pthread_mutex_unlock(&insertAndDropLock);
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
      participant->setLeaveId(freeP->leaveId);
      
      participant->doAllAddActions(true);
    }
    
    inline void drop(Participant* const participant) {
      pthread_mutex_lock(&insertAndDropLock);
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
        lastleaveId = 0;
        lastTreeNode = NULL;
        assert(firstFree == NULL);
      }
      
      pthread_mutex_unlock(&insertAndDropLock);
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
      
      // make sure that the isWaitOnly is set, getting this info is a bit
      // difficulte here, insertNode is the last leave, or it is a helper
      // this does not need to be atomic since WAIT_ONLY flags only change
      // protected by the insert lock we already acquired
      if (   (insertNode == lastTreeNode && (lastTreeNode->getMode() == WAIT_ONLY))
          || ((HelperNode*)insertNode)->flags.bits.isWaitOnly) {
        assert(!helper->flags.bits.isWaitOnly); // just make sure it was correctly initialized...
        helper->flags.bits.isWaitOnly = LEFT;
      }
      
      // use a strategy of over-announcement, i.e., insert the helper indicating
      // that the new node already synchronized. This will allow all other nodes
      // in the related subtree to announce synchronization consitently.
      // It does not pose problems since it will not imply final synchronization,
      // since the adding participant can not have synchronized yet.
      // There is no spawn-clocked/'spawn with phaser' after a resume(), which is a Good Thing(TM)
      helper->initializeSynchronized(participant->evenIteration, participant->sense);
      
      // Not modify the tree
      // this could be a raise, but it is protected by the insertLock
      insertNode->setParent(helper);
      
      
      // the participant is the least important item, and can be handled now.
      // using stack-allocation, the thread exacuting this code here,
      // should allways be the participant anyway
      participant->setParent(helper);
      
      participant->doAllAddActions(true);
      
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
    FreeParticipant* volatile firstFree;
    FreeParticipant* volatile lastFree;
    pthread_mutex_t  insertAndDropLock;
    
    // global flags
    volatile bool evenIteration;
    volatile bool sense;
    volatile bool waitFlagEven;
    volatile bool waitFlagOdd;
    
    friend class Participant;
  };
  
  inline bool setResume(HelperNodeAtomicState* const flags,
                 const bool evenIteration,
                 const bool sense) {
    // if the opponent wount do any signaling, we cannot win here
    const bool noOpponent = flags->bits.isWaitOnly;
    
    if (evenIteration) {
      if (flags->bits.winnerAlreadyReachedThisPointEven == sense)
        return true;
      
      flags->bits.winnerAlreadyReachedThisPointEven = sense;
      
    }
    else {
      if (flags->bits.winnerAlreadyReachedThisPointOdd == sense)
        return true;
      
      flags->bits.winnerAlreadyReachedThisPointOdd = sense;
    }
    
    return noOpponent;
  }
  
  inline bool propagateWaitOnly(HelperNodeAtomicState* const flags,
                         const Opponent opponent) {
    if (flags->bits.isWaitOnly != NONE) {
      // that is the old value, and only our direct opponent should have had the chance to drop this node
      assert(   (opponent == LEFT  && flags->bits.isWaitOnly == RIGHT)
             || (opponent == RIGHT && flags->bits.isWaitOnly == LEFT));
    }
    
    flags->bits.isWaitOnly = Opponent(flags->bits.isWaitOnly | opponent);
    
    // if BOTH participants have said that they are waitOnly, we need to propagate that
    return flags->bits.isWaitOnly == BOTH;
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
    //pthread_mutex_t* const lock = insertLockIfInsertNode;
    //if (lock) pthread_mutex_lock(lock);
    
    const bool evenIteration = isEvenIteration();
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
  inline bool Participant::doAllDropActions() const {
    const bool evenIteration = isEvenIteration();
    const bool sense         = getSense();
    const Mode mode          = this->mode;
          int  nodeId        = leaveId;
          HelperNode* node   = getParent();
    
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
          propagateWaitOnlyFurther = propagateWaitOnly(&newFlags,
                                                       OPPONENT_FROM_leaveId(nodeId));
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
           
      node     = node->getParent();
      nodeId >>= 1;
    }
    
    if (propagateResumeFurther) {
      notifyParticipants();
    }
    
    return propagateResumeFurther;
  }
  
  
  inline bool revokeResume(HelperNodeAtomicState* const flags, 
                    const bool evenIteration, 
                    const bool sense, 
                    int* const revokedResumes) {
    // if the opponent wount do any signaling, no-one could have won here
    // and thus, we can not count it
    bool noOpponent = flags->bits.isWaitOnly;
    
    if (evenIteration) {
      // ok we had a winner here, let's revoke it
      if (flags->bits.winnerAlreadyReachedThisPointEven == sense) {
        flags->bits.winnerAlreadyReachedThisPointEven = not sense;
        // and of course, count it
        (*revokedResumes)++;
        return true;
      }
    }
    else {
      if (flags->bits.winnerAlreadyReachedThisPointOdd == sense) {
        flags->bits.winnerAlreadyReachedThisPointOdd = not sense;
        // and count it
        (*revokedResumes)++;
        return true;
      }
    }
    
    return noOpponent;
  }
  
  /**
   * @return bool whether it was a real win
   */
  inline bool reSetResume(HelperNodeAtomicState* const flags, 
                   const bool evenIteration, 
                   const bool sense) {
    // check whether we could win here
    const bool couldWin = not flags->bits.isWaitOnly;
    
    if (evenIteration) {
      if (flags->bits.winnerAlreadyReachedThisPointEven == sense)
        return false;
      
      flags->bits.winnerAlreadyReachedThisPointEven = sense;
    }
    else {
      if (flags->bits.winnerAlreadyReachedThisPointOdd == sense)
        return false;
      
      flags->bits.winnerAlreadyReachedThisPointOdd = sense;
    }
    
    return couldWin;
  }
  

  inline bool revokeWaitOnly(HelperNodeAtomicState* const flags,
                      const Opponent opponent) {
    //printf("RevokeWaitOnly: opponent, should be left or right i.e. 1 or 2, is= %d\n", opponent);
    assert(opponent == LEFT || opponent == RIGHT);
    
    // check whether we have set our flag here
    if (flags->bits.isWaitOnly & opponent) {
      flags->bits.isWaitOnly = Opponent(flags->bits.isWaitOnly - opponent);
    }
    
    // if BOTH participants have said that they are waitOnly, we need to go further
    return flags->bits.isWaitOnly == BOTH;
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
    const bool evenIteration = isEvenIteration();
    const bool sense         = getSense();
          int  nodeId        = leaveId;
          HelperNode* node   = getParent();
    
    // ok, now let's revoke all WAIT_ONLY flags and resumes
    // count the revoked resumes
    bool revokeResumeFurther   = true;
    bool revokeWaitOnlyFurther = true;
    HelperNodeAtomicState currentFlags;
    
    // count the winning resumes, i.e., don't count the resumes where
    // was not actually won
    int revokedResumes = 0;
    
    while (node && (revokeResumeFurther || revokeWaitOnlyFurther)) {
      bool compareAndSwap_failed = true;
      currentFlags = node->flags;
      
      do {
        HelperNodeAtomicState newFlags = currentFlags;
        
        if (revokeResumeFurther) {
          revokeResumeFurther   = revokeResume(&newFlags, evenIteration, sense, &revokedResumes);
        }
        
        if (revokeWaitOnlyFurther) {
          revokeWaitOnlyFurther = revokeWaitOnly(&newFlags, OPPONENT_FROM_leaveId(nodeId));
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
      
      node     = node->getParent();
      nodeId >>= 1;
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
  : mode(SIGNAL_WAIT), resumed(false), phaser(phaser) /*, insertLockIfInsertNode(NULL)*/ {
    phaser->add(this);
  }
  
  Participant::Participant(Phaser* const phaser, const Mode mode)
  : mode(mode), resumed(false), phaser(phaser) /*, insertLockIfInsertNode(NULL)*/ {
    phaser->add(this);
  }
  
  void Participant::notifyParticipants() const {
    phaser->advanceFlags();
    if (evenIteration) {
      (*waitFlagEven) = sense;
    }
    else {
      (*waitFlagOdd)  = sense;
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
