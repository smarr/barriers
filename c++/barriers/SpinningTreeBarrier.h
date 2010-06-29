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
#include <memory>
#include <cstdlib>
#include "../misc/atomic.h"

#include "../InsertionTree/InsertionTree.h"
#include "../InsertionTree/InsertionTreeNode.h"

#ifndef BARRIER
  #define BARRIER SpinningTree::Barrier
#endif

#ifndef PARTICIPANT
  #define PARTICIPANT SpinningTree::Participant
#endif

namespace SpinningTree {

class TreeNode;
class Participant;


class Barrier : private InsertionTree<TreeNode> {
public:
  Barrier(const size_t numberOfParticipants);
  
  bool barrier(Participant* const participant);
  
  void free() {}
  
  inline void finalize_initialization() {}
  
private:
  bool waitFlagEven;
  bool waitFlagOdd;
  
  friend class Participant;
};

// it is not possible to nest that inside of the SpinningTreeBarrier class
class TreeNode : public InsertionTreeNode<TreeNode> {
public:
  TreeNode()
  : InsertionTreeNode<TreeNode>(),
    winnerAlreadyReachedThisPointFlagEven(false),
    winnerAlreadyReachedThisPointFlagOdd(false),
    dropped(false)
  {
    lock_init(&lock, NULL);
  }
  
protected:
  bool resume(Participant* const entryNode);
  
  /**
   * Returns the old value, was named testAndSetWinnerFlag() earlier.
   * Sets it to true, if it has not been set
   * @return
   */
  bool winnerAlreadyPassed(Participant* const entryNode);
  
  bool hasOpponent() const {
    return !getLeftChild()->dropped && !getRightChild()->dropped;
  }

private:
  lock_t lock;
  volatile bool winnerAlreadyReachedThisPointFlagEven;
  volatile bool winnerAlreadyReachedThisPointFlagOdd;
  
  const bool dropped; //REMOVE CONST when drop is implemented
  
};

class Participant : public TreeNode {
public:
  Participant(Barrier* const barrier)
  : TreeNode(),
    waitFlagEven(&barrier->waitFlagEven),
    waitFlagOdd(&barrier->waitFlagOdd),
    evenIteration(true),
    sense(true)
  {
    barrier->addNode(this);
  }
  
  bool resume() {
    if (resumed) {
      return false;
    }
    else {
      resumed = true;
    }
    
    return TreeNode::resume(this);
  }
  
  bool next() {
    if (evenIteration) {
      while (sense != *waitFlagEven);
    }
    else {
      while (sense != *waitFlagOdd);
    }
    
    resumed = false;
    evenIteration = !evenIteration;
    if (evenIteration) {
      sense = !sense;
    }
    
    return false;
  }

  void notifyParticipants() const {
    if (evenIteration) {
      *waitFlagEven = sense;
    }
    else {
      *waitFlagOdd  = sense;
    }
    memory_fence();
  }
  
  inline bool getEvenIteration() const { return evenIteration; }
  inline bool getSense()         const { return sense; }

private:
  bool* const waitFlagEven;
  bool* const waitFlagOdd;
  
  bool evenIteration;
  bool sense;
  
  bool resumed;
};


bool Barrier::barrier(Participant* const participant) {
  bool resumed = participant->resume();
  participant->next();
  return resumed;
}

Barrier::Barrier(const size_t numberOfParticipants) : waitFlagEven(false), waitFlagOdd(false) {}
  
  
bool TreeNode::resume(Participant* const entryNode) {
  // Do the announcement that synchronization for that activity is reached
  if (getParent() == NULL) {
    // That is the root node
    // so lets notify all participants
    entryNode->notifyParticipants();
    return true;
  }

  if (getParent()->winnerAlreadyPassed(entryNode)) {
    return getParent()->resume(entryNode);
  }
  
  return false;
}
  
bool TreeNode::winnerAlreadyPassed(Participant* const entryNode) {
  // Maybe opponent has been dropped from the barrier, then we cannt win here
  if (!hasOpponent()) return true;
  
  // Here we depend on ugly code.
  // Two flags are needed to support resume/fuzzy barriers.
  
  const bool sense = entryNode->getSense();
  
  if (entryNode->getEvenIteration()) {
    // simple optimization??
    if (winnerAlreadyReachedThisPointFlagEven == sense) {
      return true;
    }
    
    lock_acquire(&lock);
    // needs to be here, flag could have been changed before reaching the critical section
    if (winnerAlreadyReachedThisPointFlagEven == sense) {
      lock_release(&lock);
      return true;
    }
    else {
      winnerAlreadyReachedThisPointFlagEven = sense;
      lock_release(&lock);
      return false;
    }
  }
  else {
    // simple optimization??
    if (winnerAlreadyReachedThisPointFlagOdd == sense) {
      return true;
    }
    
    lock_acquire(&lock);
    // needs to be here, flag could have been changed before reaching the critical section
    if (winnerAlreadyReachedThisPointFlagOdd == sense) {
      lock_release(&lock);
      return true;
    }
    else {
      winnerAlreadyReachedThisPointFlagOdd = sense;
      lock_release(&lock);
      return false;
    }
  }
}

}

