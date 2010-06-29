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
bool InsertionTree<ConcreteNodeType>::contains(ConcreteNodeType* obj) const {
  return ConcreteNodeType::treeContains(rootNode, obj);
}


template <class ConcreteNodeType>
void InsertionTree<ConcreteNodeType>::addNode(ConcreteNodeType* const node) {
  // TODO: this traversal might not be safe
  ConcreteNodeType* const insertNode = insertionNode();
 
  // TODO: find out whether we can do that without a global mutex
  pthread_mutex_lock(&lock);
 
  if (insertNode == NULL) {
    rootNode = node;
    firstNode = node;
    lastNode = node;

    _height = 1;

    numOfLeaves = 1;
    numOfNodes = 1;

    pthread_mutex_unlock(&lock);
    return;
  }
 
  // The general case
  ConcreteNodeType* const tmpNode = new ConcreteNodeType();
  tmpNode->setLeftChild(insertNode);
  tmpNode->setRightChild(node);

  if (insertNode != rootNode) {
    tmpNode->setParent(insertNode->getParent());
    insertNode->getParent()->setRightChild(tmpNode);
  }

  insertNode->setParent(tmpNode);
  node->setParent(tmpNode);

  // reset root and others
  if (insertNode == rootNode) {
    rootNode = tmpNode;
    _height++;
  }

  lastNode = node;

  // set tree characteristics
  numOfLeaves++;
  numOfNodes += 2;

  pthread_mutex_unlock(&lock);
  return;
}
 

template <class ConcreteNodeType>
ConcreteNodeType* InsertionTree<ConcreteNodeType>::insertionNode() {
  size_t tmp = numOfLeaves;

  if (tmp == 0) { return NULL; }

  ConcreteNodeType* result = lastNode;

  while (tmp % 2 == 0) /* (tmp & 1) == 0  <-- might perform better */ {
    result = result->getParent();
    tmp = tmp / 2; // or a bit shift???
  }

  return result;
}

template <class ConcreteNodeType>
void InsertionTree<ConcreteNodeType>::free() {
  // TODO NOTYETIMPLEMENTED
}


/** public boolean removeNode(final TreeNode element) {
  // TODO Auto-generated method stub
  return false;
} */
