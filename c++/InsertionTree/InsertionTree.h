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


#ifndef __INSERTION_TREE_H__
#define __INSERTION_TREE_H__

#include <cstddef>
#include <pthread.h>

template <class NodeType>
class InsertionTree {
public:
  
  InsertionTree() : rootNode(NULL), firstNode(NULL), lastNode(NULL),
                    _height(0), numOfNodes(0), numOfLeaves(0) {
    pthread_mutex_init(&lock, NULL);
  }
  
  void addNode(NodeType* const node);

  bool contains(NodeType* obj) const;
  
  inline NodeType* root()  const { return rootNode; }
  inline NodeType* first() const { return firstNode; }
  inline NodeType* last()  const { return lastNode;  }
    
  inline size_t size() const          { return numOfLeaves; }
  inline size_t numberOfNodes() const { return numOfNodes; }
  inline size_t height() const        { return _height; }
  
  inline bool isPerfect() const {
    return numOfLeaves == 0
          || (numOfLeaves & (numOfLeaves - 1)) == 0; // is power of 2
  }
  
  void free();

private:
  NodeType* rootNode;
  NodeType* firstNode;
  NodeType* lastNode;
  
  size_t _height;
  size_t numOfNodes;
  size_t numOfLeaves;
  
  pthread_mutex_t lock;
  
  NodeType* insertionNode();
};

#include "InsertionTree.impl.h"

#endif
