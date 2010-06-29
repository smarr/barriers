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


#ifndef __INSERTION_TREE_WITH_VALUE_H__
#define __INSERTION_TREE_WITH_VALUE_H__

#include <cstddef>

#warning We include <vector> here. \
         There is an urban ledgend which says that this \
         might have unintended impacts. \
         It needs to be validate that a version without the as_vector functionallity does not perform better

#include <vector>

#include "InsertionTree.h"

using namespace std;

template <class NodeType, typename ValueType>
class InsertionTreeWithValue : public InsertionTree<NodeType> {
public:
  
  InsertionTreeWithValue() : InsertionTree<NodeType>() {}
  
  NodeType* add(ValueType& value);
  
  bool contains(ValueType& obj) const;
  
  inline ValueType* first() const { 
    NodeType* firstNode = InsertionTree<NodeType>::first();
    return (firstNode == NULL) ? NULL : firstNode->getValue(); }
  inline ValueType* last()  const {
    NodeType* lastNode = InsertionTree<NodeType>::last();
    return (lastNode  == NULL) ? NULL : lastNode->getValue();  }
  
  /**
     Should be fine to do such a copy here, its only used for testing anyway,
     and we don't have to care about the memory...
   **/
  vector<ValueType*> as_vector() const;
  
  void free();

private:
  size_t collectLeaves(NodeType* const node, vector<ValueType*>* result, size_t i) const;
};

#include "InsertionTreeWithValue.impl.h"

#endif
