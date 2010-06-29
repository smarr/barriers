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


template<class ConcreteNodeType>
class InsertionTreeNode {
public:
  InsertionTreeNode() : leftChild(NULL), rightChild(NULL), parent(NULL) {}
  
  inline bool isLeaf() const { return leftChild == NULL && rightChild == NULL; }
  
  inline ConcreteNodeType* getLeftChild() const  { return leftChild; }
  inline ConcreteNodeType* getRightChild() const { return rightChild; }
  inline ConcreteNodeType* getParent() const     { return parent; }
  
  inline void setLeftChild(ConcreteNodeType* const node) { leftChild  = node; }
  inline void setRightChild(ConcreteNodeType* const node){ rightChild = node; }
  inline void setParent(ConcreteNodeType* const node)    { parent     = node; }
  
  static bool treeContains(ConcreteNodeType* const node, ConcreteNodeType* const obj);
  
private:
  ConcreteNodeType* leftChild;
  ConcreteNodeType* rightChild;
  ConcreteNodeType* parent;
};

#include "InsertionTreeNode.impl.h"

