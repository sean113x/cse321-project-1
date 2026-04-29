#include "bplustree.h"

#include <stdexcept>

BPlusTree::Entry::Entry(int key, int rid) : key(key), rid(rid) {}

BPlusTree::Node::Node(bool isLeaf) : isLeaf(isLeaf) {}

BPlusTree::InternalNode::InternalNode() : Node(false) {}

BPlusTree::LeafNode::LeafNode() : Node(true), next(nullptr) {}

BPlusTree::BPlusTree(int order)
    : IndexTree(order), root(nullptr), numInternalKey(0) {
  if (order < 3) {
    throw std::invalid_argument("BPlusTree order must be at least 3.");
  }
}

BPlusTree::~BPlusTree() {
  auto clear = [](auto &&self, Node *node) -> void {
    if (node == nullptr) {
      return;
    }
    if (!node->isLeaf) {
      auto *internal = static_cast<InternalNode *>(node);
      for (Node *child : internal->children) {
        self(self, child);
      }
    }
    delete node;
  };
  clear(clear, root);
}

/*
  Main functions:
  - search(): retrieve the key-rid pair from the leaf node.
  - range_query(): retrieve the key-rid pairs in the leafs.
  - insert(): insert the key-rid pair into leaf node.
  - remove(): remove the key-rid pair in leaf node.
*/

int BPlusTree::minEntriesLeaf() const { return (maxEntries() + 1) / 2; }

int BPlusTree::search(int key) const {
  auto [leaf, index] = search(root, key);

  if (leaf == nullptr) {
    return -1;
  }

  if (index < static_cast<int>(leaf->entries.size()) &&
      leaf->entries[index].key == key) {
    return leaf->entries[index].rid;
  }

  return -1;
}

int BPlusTree::calculateHeight() const {
  int height = 0;
  Node *current = root;

  while (current != nullptr) {
    height++;

    if (current->isLeaf) {
      break;
    }

    InternalNode *internal = static_cast<InternalNode *>(current);
    if (internal->children.empty()) {
      break;
    }

    current = internal->children[0];
  }

  return height;
}

bool BPlusTree::isUnderfull(Node *node) const {
  if (node == root) {
    return false;
  }

  if (node->isLeaf) {
    auto *leaf = static_cast<LeafNode *>(node);
    return static_cast<int>(leaf->entries.size()) < minEntriesLeaf();
  }

  auto *internal = static_cast<InternalNode *>(node);
  return static_cast<int>(internal->keys.size()) < minEntries();
}

double BPlusTree::overallNodeUtilization() const {
  if (numNode == 0) {
    return 0.0;
  }

  return static_cast<double>(numEntry + numInternalKey) /
         static_cast<double>(numNode * maxEntries());
}

double BPlusTree::getLeafNodeUtilization() const {
  if (root == nullptr) {
    return 0.0;
  }

  Node *current = root;
  while (!current->isLeaf) {
    auto *internal = static_cast<InternalNode *>(current);
    current = internal->children.front();
  }

  int numLeafNode = 0;
  auto *leaf = static_cast<LeafNode *>(current);

  while (leaf != nullptr) {
    numLeafNode++;
    leaf = leaf->next;
  }

  return 100.0 * numEntry / (numLeafNode * maxEntries());
}

double BPlusTree::getNodeUtilization() const {
  return 100.0 * overallNodeUtilization();
}

std::vector<int> BPlusTree::range_query(int startKey, int endKey) const {
  std::vector<int> rids;

  if (startKey > endKey) {
    return rids;
  }

  auto [leaf, index] = search(root, startKey);

  while (leaf != nullptr) {
    while (index < static_cast<int>(leaf->entries.size())) {
      if (leaf->entries[index].key > endKey) {
        return rids;
      }

      rids.push_back(leaf->entries[index].rid);
      index++;
    }

    leaf = leaf->next;
    index = 0;
  }

  return rids;
}

void BPlusTree::insert(int key, int rid) {
  Entry newEntry(key, rid);

  if (root == nullptr) {
    auto *leaf = new LeafNode();
    leaf->entries.push_back(newEntry);
    root = leaf;
    numNode++;
    numEntry++;
    return;
  }

  std::vector<std::pair<InternalNode *, int>> path;
  Node *current = root;

  while (!current->isLeaf) {
    auto *internal = static_cast<InternalNode *>(current);
    int index = findIndex(internal->keys, key);

    if (index < static_cast<int>(internal->keys.size()) &&
        internal->keys[index] == key) {
      ++index;
    }

    path.push_back({internal, index});
    current = internal->children[index];
  }

  auto *leaf = static_cast<LeafNode *>(current);
  int index = findIndex(leaf->entries, key);

  // If the key already exists in a leaf node, do not insert.
  if (index < static_cast<int>(leaf->entries.size()) &&
      leaf->entries[index].key == key) {
    return;
  }

  leaf->entries.insert(leaf->entries.begin() + index, newEntry);
  numEntry++;
  handleOverflow(leaf, path);
}

void BPlusTree::remove(int key) {
  if (root == nullptr) {
    return;
  }

  std::vector<std::pair<InternalNode *, int>> path;
  Node *current = root;

  while (!current->isLeaf) {
    auto *internal = static_cast<InternalNode *>(current);
    int index = findIndex(internal->keys, key);

    if (index < static_cast<int>(internal->keys.size()) &&
        internal->keys[index] == key) {
      ++index;
    }

    path.push_back({internal, index});
    current = internal->children[index];
  }

  auto *leaf = static_cast<LeafNode *>(current);
  int index = findIndex(leaf->entries, key);

  if (index >= static_cast<int>(leaf->entries.size()) ||
      leaf->entries[index].key != key) {
    return;
  }

  leaf->entries.erase(leaf->entries.begin() + index);
  numEntry--;

  if (index == 0 && !leaf->entries.empty() && !path.empty()) {
    auto [parent, childIndex] = path.back();
    if (childIndex > 0) {
      parent->keys[childIndex - 1] = leaf->entries.front().key;
    }
  }

  handleUnderflow(leaf, path);
}

/*
  Helper functions:
  - findIndex(): find the first key that is >= the given key.
  - search(): recursively find the leaf node that may contain the key.
  - splitNode(): split the node and return the separator. (copy up for leaf
  node)
  - handleOverflow(): handle the overflow by splitting node.
  - concatenation(): merge the child and its right sibling.
  - redistribution(): redistribute entries between the child and its right
  sibling.
  - handleUnderflow(): handle the underflow by concatenation or redistribution.
*/

int BPlusTree::findIndex(const std::vector<int> &keys, int key) const {
  int index = 0;

  while (index < static_cast<int>(keys.size()) && keys[index] < key) {
    ++index;
  }

  return index;
}

int BPlusTree::findIndex(const std::vector<Entry> &entries, int key) const {
  int index = 0;

  while (index < static_cast<int>(entries.size()) && entries[index].key < key) {
    ++index;
  }

  return index;
}

std::pair<BPlusTree::LeafNode *, int> BPlusTree::search(Node *node,
                                                        int key) const {
  if (node == nullptr) {
    return {nullptr, -1};
  }

  if (node->isLeaf) { // It must reach to a leaf node.
    auto *leaf = static_cast<LeafNode *>(node);
    return {leaf, findIndex(leaf->entries, key)};
  }

  auto *internal = static_cast<InternalNode *>(node);
  int index = findIndex(internal->keys, key);

  if (index < static_cast<int>(internal->keys.size()) &&
      internal->keys[index] == key) {
    ++index;
  }

  return search(internal->children[index], key);
}

int BPlusTree::splitNode(Node *node, Node *&rightNode) {
  if (node->isLeaf) {
    auto *leaf = static_cast<LeafNode *>(node);
    auto *rightLeaf = new LeafNode();
    numNode++;
    int mid = static_cast<int>(leaf->entries.size()) / 2;

    rightLeaf->entries.assign(leaf->entries.begin() + mid, leaf->entries.end());
    leaf->entries.erase(leaf->entries.begin() + mid, leaf->entries.end());

    rightLeaf->next = leaf->next;
    leaf->next = rightLeaf;
    rightNode = rightLeaf;

    splitCount++;
    return rightLeaf->entries.front().key; // leaf split uses copy-up
  }

  auto *internal = static_cast<InternalNode *>(node);
  auto *rightInternal = new InternalNode();
  numNode++;
  int mid = static_cast<int>(internal->keys.size()) / 2;
  int upKey = internal->keys[mid];

  rightInternal->keys.assign(internal->keys.begin() + mid + 1,
                             internal->keys.end());
  rightInternal->children.assign(internal->children.begin() + mid + 1,
                                 internal->children.end());

  internal->keys.erase(internal->keys.begin() + mid, internal->keys.end());
  internal->children.resize(mid + 1);
  rightNode = rightInternal;

  splitCount++;
  return upKey; // internal split uses move-up
}

void BPlusTree::handleOverflow(
    Node *node, std::vector<std::pair<InternalNode *, int>> &path) {
  auto entryCount = [](Node *current) {
    if (current->isLeaf) {
      return static_cast<int>(static_cast<LeafNode *>(current)->entries.size());
    }

    return static_cast<int>(static_cast<InternalNode *>(current)->keys.size());
  };

  while (entryCount(node) > maxEntries()) {
    bool splitWasLeaf = node->isLeaf;
    Node *rightNode = nullptr;
    int upKey = splitNode(node, rightNode);

    if (path.empty()) {
      auto *newRoot = new InternalNode();
      numNode++;
      newRoot->keys.push_back(upKey);
      newRoot->children.push_back(node);
      newRoot->children.push_back(rightNode);
      root = newRoot;
      if (splitWasLeaf) {
        numInternalKey++;
      }
      return;
    }

    auto [parent, childIndex] = path.back();
    path.pop_back();

    parent->keys.insert(parent->keys.begin() + childIndex, upKey);
    parent->children.insert(parent->children.begin() + childIndex + 1,
                            rightNode);
    if (splitWasLeaf) {
      numInternalKey++;
    }
    node = parent;
  }
}

void BPlusTree::concatenation(InternalNode *parent, int leftIndex) {
  Node *left = parent->children[leftIndex];
  Node *right = parent->children[leftIndex + 1];
  int separator = parent->keys[leftIndex];

  parent->keys.erase(parent->keys.begin() + leftIndex);
  parent->children.erase(parent->children.begin() + leftIndex + 1);

  if (left->isLeaf) {
    auto *leftLeaf = static_cast<LeafNode *>(left);
    auto *rightLeaf = static_cast<LeafNode *>(right);

    leftLeaf->entries.insert(leftLeaf->entries.end(),
                             rightLeaf->entries.begin(),
                             rightLeaf->entries.end());
    leftLeaf->next = rightLeaf->next;
    numInternalKey--; // leaf merge removes a parent separator
  } else {
    auto *leftInternal = static_cast<InternalNode *>(left);
    auto *rightInternal = static_cast<InternalNode *>(right);

    // Internal merge moves the parent separator down into the merged node.
    leftInternal->keys.push_back(separator);
    leftInternal->keys.insert(leftInternal->keys.end(),
                              rightInternal->keys.begin(),
                              rightInternal->keys.end());
    leftInternal->children.insert(leftInternal->children.end(),
                                  rightInternal->children.begin(),
                                  rightInternal->children.end());
  }

  delete right;
  numNode--;
}

void BPlusTree::redistribution(InternalNode *parent, int leftIndex) {
  Node *left = parent->children[leftIndex];
  Node *right = parent->children[leftIndex + 1];

  if (left->isLeaf) {
    auto *leftLeaf = static_cast<LeafNode *>(left);
    auto *rightLeaf = static_cast<LeafNode *>(right);

    std::vector<Entry> entries = leftLeaf->entries;
    entries.insert(entries.end(), rightLeaf->entries.begin(),
                   rightLeaf->entries.end());

    int mid = static_cast<int>(entries.size()) / 2;

    leftLeaf->entries.assign(entries.begin(), entries.begin() + mid);
    rightLeaf->entries.assign(entries.begin() + mid, entries.end());
    parent->keys[leftIndex] = rightLeaf->entries.front().key;
    return;
  }

  auto *leftInternal = static_cast<InternalNode *>(left);
  auto *rightInternal = static_cast<InternalNode *>(right);

  std::vector<int> keys = leftInternal->keys;
  keys.push_back(parent->keys[leftIndex]);
  keys.insert(keys.end(), rightInternal->keys.begin(),
              rightInternal->keys.end());

  std::vector<Node *> children = leftInternal->children;
  children.insert(children.end(), rightInternal->children.begin(),
                  rightInternal->children.end());

  int mid = static_cast<int>(keys.size()) / 2;

  leftInternal->keys.assign(keys.begin(), keys.begin() + mid);
  parent->keys[leftIndex] = keys[mid];
  rightInternal->keys.assign(keys.begin() + mid + 1, keys.end());

  int leftChildCount = static_cast<int>(leftInternal->keys.size()) + 1;

  leftInternal->children.assign(children.begin(),
                                children.begin() + leftChildCount);
  rightInternal->children.assign(children.begin() + leftChildCount,
                                 children.end());
}

void BPlusTree::handleUnderflow(
    Node *node, std::vector<std::pair<InternalNode *, int>> &path) {
  while (node != root && isUnderfull(node)) {
    if (path.empty()) {
      break;
    }

    auto [parent, childIndex] = path.back();
    path.pop_back();

    // Use right sibling if possible; otherwise use left sibling.
    int leftIndex = (childIndex < static_cast<int>(parent->children.size()) - 1)
                        ? childIndex
                        : childIndex - 1;

    Node *left = parent->children[leftIndex];
    Node *right = parent->children[leftIndex + 1];

    if (left->isLeaf) {
      auto *leftLeaf = static_cast<LeafNode *>(left);
      auto *rightLeaf = static_cast<LeafNode *>(right);
      int total = static_cast<int>(leftLeaf->entries.size()) +
                  static_cast<int>(rightLeaf->entries.size());

      if (total <= maxEntries()) {
        concatenation(parent, leftIndex);
        node = parent;
      } else {
        redistribution(parent, leftIndex);
        return;
      }
    } else {
      auto *leftInternal = static_cast<InternalNode *>(left);
      auto *rightInternal = static_cast<InternalNode *>(right);
      int total = static_cast<int>(leftInternal->keys.size()) + 1 +
                  static_cast<int>(rightInternal->keys.size());

      if (total <= maxEntries()) {
        concatenation(parent, leftIndex);
        node = parent;
      } else {
        redistribution(parent, leftIndex);
        return;
      }
    }
  }

  if (root == nullptr) {
    return;
  }

  if (root->isLeaf) {
    auto *leaf = static_cast<LeafNode *>(root);

    if (leaf->entries.empty()) {
      delete root;
      root = nullptr;
      numNode--;
    }

    return;
  }

  auto *internal = static_cast<InternalNode *>(root);

  if (internal->keys.empty()) {
    Node *oldRoot = root;
    root = internal->children.front();
    delete oldRoot;
    numNode--;
  }
}
