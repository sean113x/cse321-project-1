#include "btree.h"

#include <stdexcept>

BTree::Entry::Entry(int key, int rid)
    : key(key), rid(rid) {}

BTree::Node::Node(bool isLeaf)
    : isLeaf(isLeaf) {}

BTree::BTree(int order)
    : IndexTree(order), root(nullptr) {
    if (order < 3) {
        throw std::invalid_argument("BTree order must be at least 3.");
    }
}

BTree::~BTree() {
    auto clear = [](auto&& self, Node* node) -> void {
        if (node == nullptr) { return; }
        for (Node* child : node->children) { self(self, child); }
        delete node;
    };
    clear(clear, root);
}

/*
  Main functions:
  - search(): return the rid associated with the given key, or -1 if not found.
  - insert(): insert the key-rid pair into the B-Tree.
  - remove(): remove the key and its associated rid from the B-Tree. If the key does not exist, do nothing.
*/

int BTree::search(int key) const {
    return search(root, key);
}

void BTree::insert(int key, int rid) {
  Entry newEntry(key, rid);

  if (root == nullptr) {
    root = new Node(true);
    root->entries.push_back(newEntry);
    return;
  }

  std::vector<std::pair<Node*, int>> path;
  Node* current = root;

  while (!current->isLeaf) {
    int index = findIndex(current->entries, key);

    // If the key already exists in an internal node, do not insert
    if (index < static_cast<int>(current->entries.size()) &&
        current->entries[index].key == key) { return; }
    path.push_back({current, index});
    current = current->children[index];
  }

  int index = findIndex(current->entries, key);

  // If the key already exists in a leaf node, do not insert
  if (index < static_cast<int>(current->entries.size()) && 
      current->entries[index].key == key) { return; }

  current->entries.insert(current->entries.begin() + index, newEntry);
  handleOverflow(current, path);
}

void BTree::remove(int key) {
  if (root == nullptr) { return; }

  std::vector<std::pair<Node*, int>> path;
  Node* current = root;

  while (true) {
    int index = findIndex(current->entries, key);

    if (index < static_cast<int>(current->entries.size()) &&
        current->entries[index].key == key) {
      if (!current->isLeaf) {
        Node* successor = current->children[index + 1];
        path.push_back({current, index + 1});

        while (!successor->isLeaf) {
          path.push_back({successor, 0});
          successor = successor->children[0];
        }

        current->entries[index] = successor->entries.front();
        current = successor;
        index = 0;
      }

      current->entries.erase(current->entries.begin() + index);
      handleUnderflow(current, path);
      return;
    }

    if (current->isLeaf) { return; }

    path.push_back({current, index});
    current = current->children[index];
  }
}

/*
  Helper functions:
  - findIndex(): return the index of the first entry whose key is equal to or greater than to the given key.
  - search(): recursive helper for search().
  - splitNode(): split the given node and return the promoted entry.
  - handleOverflow(): check if the node has overflowed.
  - concatenation(): concatenate the child at childIndex with its sibling.
  - redistribution(): redistribute entries between the child at childIndex and its sibling.
  - handleUnderflow(): check if the node has under-flowed.
*/

int BTree::findIndex(const std::vector<Entry>& entries, int key) const {
  int index = 0;

  while (index < static_cast<int>(entries.size()) && entries[index].key < key) {
    ++index;
  }

  return index;
}

int BTree::search(Node* node, int key) const {
  if (node == nullptr) { return -1; }
  int index = findIndex(node->entries, key);

  if (index < static_cast<int>(node->entries.size()) &&
    node->entries[index].key == key) {
    return node->entries[index].rid;
  }

  if (node->isLeaf) { return -1; }

  return search(node->children[index], key);
}

BTree::Entry BTree::splitNode(Node* node, Node*& rightNode) {
  int entryCount = static_cast<int>(node->entries.size());
  int mid = entryCount / 2;

  Entry upEntry = node->entries[mid];
  rightNode = new Node(node->isLeaf);

  for (int i = mid + 1; i < entryCount; i++) {
    rightNode->entries.push_back(node->entries[i]);
  }

  if (!node->isLeaf) {
    int childCount = static_cast<int>(node->children.size());

    for (int i = mid + 1; i < childCount; i++) {
      rightNode->children.push_back(node->children[i]);
    }
  }

  node->entries.erase(node->entries.begin() + mid, node->entries.end());
  if (!node->isLeaf) { node->children.resize(mid + 1); }

  splitCount++;
  return upEntry;
}

void BTree::handleOverflow(Node* node, std::vector<std::pair<Node*, int>>& path) {
  while (static_cast<int>(node->entries.size()) > maxEntries()) {
    Node* rightNode = nullptr;
    Entry upEntry = splitNode(node, rightNode);
    
    if (path.empty()) { // If the root node overflows, split it into two nodes.
      root = new Node(false);
      
      root->entries.push_back(upEntry);
      root->children.push_back(node);
      root->children.push_back(rightNode);
      return;
    }

    auto [parent, childIndex] = path.back();
    path.pop_back();

    parent->entries.insert(parent->entries.begin() + childIndex, upEntry);
    parent->children.insert(parent->children.begin() + childIndex + 1, rightNode);
    node = parent;
  }
}

void BTree::concatenation(Node* parent, int leftIndex) {
  Node* leftChild = parent->children[leftIndex];
  Node* rightChild = parent->children[leftIndex + 1];

  leftChild->entries.push_back(parent->entries[leftIndex]);
  for (const Entry& entry : rightChild->entries) {
    leftChild->entries.push_back(entry);
  }

  if (!leftChild->isLeaf) { // Internal node case: concatenate child pointers too.
    for (Node* child : rightChild->children) {
      leftChild->children.push_back(child);
    }
  }

  parent->entries.erase(parent->entries.begin() + leftIndex);
  parent->children.erase(parent->children.begin() + leftIndex + 1);

  delete rightChild;
}

void BTree::redistribution(Node* parent, int leftIndex) {
  Node* left = parent->children[leftIndex];
  Node* right = parent->children[leftIndex + 1];

  std::vector<Entry> entries;
  entries.reserve(left->entries.size() + 1 + right->entries.size());

  entries.insert(entries.end(), left->entries.begin(), left->entries.end());
  entries.push_back(parent->entries[leftIndex]);
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int mid = static_cast<int>(entries.size()) / 2;

  left->entries.assign(entries.begin(), entries.begin() + mid);
  parent->entries[leftIndex] = entries[mid];
  right->entries.assign(entries.begin() + mid + 1, entries.end());

  if (!left->isLeaf) {
    std::vector<Node*> children;
    children.reserve(left->children.size() + right->children.size());

    children.insert(children.end(), left->children.begin(), left->children.end());
    children.insert(children.end(), right->children.begin(), right->children.end());

    int leftChildCount = static_cast<int>(left->entries.size()) + 1;

    left->children.assign(children.begin(), children.begin() + leftChildCount);
    right->children.assign(children.begin() + leftChildCount, children.end());
  }
}

void BTree::handleUnderflow(Node* node, std::vector<std::pair<Node*, int>>& path) {
  while (node != root &&
         static_cast<int>(node->entries.size()) < minEntries()) {
    auto [parent, childIndex] = path.back();
    path.pop_back();

    // Use right sibling if possible; otherwise use left sibling.
    int leftIndex =
        (childIndex < static_cast<int>(parent->children.size()) - 1)
            ? childIndex
            : childIndex - 1;

    Node* left = parent->children[leftIndex];
    Node* right = parent->children[leftIndex + 1];

    int total =
        static_cast<int>(left->entries.size()) +
        1 +
        static_cast<int>(right->entries.size());

    if (total <= maxEntries()) { // Underflow by parent node or not.
      concatenation(parent, leftIndex);
      node = parent;
    } else {
      redistribution(parent, leftIndex);
      return;
    }
  }

  if (root != nullptr && root->entries.empty()) {
    Node* oldRoot = root;

    if (root->isLeaf) {
      root = nullptr;
    } else {
      root = root->children[0];
    }

    delete oldRoot;
  }
}
