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


template <class NodeType, typename ValueType>
bool InsertionTreeWithValue<NodeType, ValueType>::contains(ValueType& obj) const {
  return NodeType::treeContains(this->root(), obj);
}

template <class NodeType, typename ValueType>
vector<ValueType*> InsertionTreeWithValue<NodeType, ValueType>::as_vector() const {
  vector<ValueType*> result(this->size());
  
  this->collectLeaves(this->root(), &result, 0);
  
  return result;
}

template <class NodeType, typename ValueType>
size_t InsertionTreeWithValue<NodeType, ValueType>::collectLeaves(NodeType* const node, vector<ValueType*>* const result, size_t i) const {
  if (node->isLeaf()) {
    (*result)[i] = node->getValue();
    
    return i + 1;
  }
  
  i = collectLeaves(node->getLeftChild(), result, i);
  if (node->getRightChild() != NULL) {
    i = collectLeaves(node->getRightChild(), result, i);
  }
  
  return i;
}



template <class NodeType, typename ValueType>
NodeType* InsertionTreeWithValue<NodeType, ValueType>::add(ValueType& value) {
  NodeType* node = new NodeType(value);
 
  addNode(node);
 
  return node;
}

template <class NodeType, typename ValueType>
void InsertionTreeWithValue<NodeType, ValueType>::free() {
  // TODO NOTYETIMPLEMENTED
}


/** public boolean removeNode(final TreeNode element) {
  // TODO Auto-generated method stub
  return false;
} */
