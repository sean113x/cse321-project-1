#include "bstartree.h"

#include <stdexcept>

BStarTree::Entry::Entry(int key, int rid)
    : key(key), rid(rid) {}

BStarTree::Node::Node(bool isLeaf)
    : isLeaf(isLeaf) {}

BStarTree::BStarTree(int order)
    : IndexTree(order), root(nullptr) {
    if (order < 3) {
        throw std::invalid_argument("BStarTree order must be at least 3.");
    }
}

BStarTree::~BStarTree() {
    auto clear = [](auto&& self, Node* node) -> void {
        if (node == nullptr) { return; }
        for (Node* child : node->children) { self(self, child); }
        delete node;
    };
    clear(clear, root);
}

/*
  Main functions: (same as BTree)
  - search(): return the rid associated with the given key, or -1 if not found.
  - insert(): insert the key-rid pair into the B*-Tree.
  - remove(): remove the key and its associated rid from the B*-Tree. If the key does not exist, do nothing.
*/

int BStarTree::search(int key) const {
    return search(root, key);
}

void BStarTree::insert(int key, int rid) {
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

void BStarTree::remove(int key) {
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
  - splitNode(): split two full sibling nodes into three nodes.
  - redistributeOverflow(): redistribute entries with a sibling before splitting.
  - handleOverflow(): check if the node has overflowed.
  - concatenation(): concatenate the child at childIndex with its sibling.
  - redistributeUnderflow(): redistribute entries between the child at childIndex and its sibling.
  - handleUnderflow(): check if the node has underflowed.
*/

int BStarTree::findIndex(const std::vector<Entry>& entries, int key) const { // same as B-tree
  int index = 0;

  while (index < static_cast<int>(entries.size()) && entries[index].key < key) {
    ++index;
  }

  return index;
}

int BStarTree::search(Node* node, int key) const { // same as B-tree
  if (node == nullptr) { return -1; }
  int index = findIndex(node->entries, key);

  if (index < static_cast<int>(node->entries.size()) &&
      node->entries[index].key == key) {
    return node->entries[index].rid;
  }

  if (node->isLeaf) { return -1; }

  return search(node->children[index], key);
}

void BStarTree::splitNode(Node* parent, int leftIndex) {
  Node* left = parent->children[leftIndex];
  Node* right = parent->children[leftIndex + 1];
  Node* middle = new Node(left->isLeaf);

  std::vector<Entry> entries = left->entries;
  entries.push_back(parent->entries[leftIndex]); // Add the separating entry from the parent.
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int entryCount = static_cast<int>(entries.size()) - 2; // 2 separator.
  int firstUpIndex = (entryCount + 2) / 3;
  int secondUpIndex = firstUpIndex + 1 + (entryCount + 1) / 3;

  // Distribute entries to left, middle, and right nodes. Promote two separators to the parent node.
  left->entries.assign(entries.begin(), entries.begin() + firstUpIndex);
  parent->entries[leftIndex] = entries[firstUpIndex];
  middle->entries.assign(entries.begin() + firstUpIndex + 1, entries.begin() + secondUpIndex);
  parent->entries.insert(parent->entries.begin() + leftIndex + 1, entries[secondUpIndex]);
  right->entries.assign(entries.begin() + secondUpIndex + 1, entries.end());

  if (!left->isLeaf) { // Internal node case: distribute child pointers as well.
    std::vector<Node*> children = left->children;
    children.insert(children.end(), right->children.begin(), right->children.end());

    int firstChildEnd = firstUpIndex + 1;
    int secondChildEnd = secondUpIndex + 1;

    left->children.assign(children.begin(), children.begin() + firstChildEnd);
    middle->children.assign(children.begin() + firstChildEnd, children.begin() + secondChildEnd);
    right->children.assign(children.begin() + secondChildEnd, children.end());
  }

  parent->children.insert(parent->children.begin() + leftIndex + 1, middle);
  splitCount++;
}

void BStarTree::handleOverflow(Node* node, std::vector<std::pair<Node*, int>>& path) {
  while (static_cast<int>(node->entries.size()) > maxEntries()) {
    if (path.empty()) { // If the root node overflows, split it into two nodes.
      int mid = static_cast<int>(node->entries.size()) / 2;
      Entry upEntry = node->entries[mid];
      Node* rightNode = new Node(node->isLeaf);

      rightNode->entries.assign(node->entries.begin() + mid + 1, node->entries.end());
      node->entries.erase(node->entries.begin() + mid, node->entries.end());

      if (!node->isLeaf) {
        rightNode->children.assign(node->children.begin() + mid + 1, node->children.end());
        node->children.resize(mid + 1);
      }

      root = new Node(false);
      root->entries = {upEntry};
      root->children = {node, rightNode};

      splitCount++;
      return;
    }

    auto [parent, childIndex] = path.back();
    path.pop_back();

    if (redistributeOverflow(parent, childIndex)) {
      return;
    }

    int leftIndex =
        (childIndex < static_cast<int>(parent->children.size()) - 1)
            ? childIndex
            : childIndex - 1;

    splitNode(parent, leftIndex);
    node = parent;
  }
}

bool BStarTree::redistributeOverflow(Node* parent, int childIndex) {
  int leftIndex = -1;

  if (childIndex < static_cast<int>(parent->children.size()) - 1 &&
      static_cast<int>(parent->children[childIndex + 1]->entries.size()) < maxEntries()) {
    leftIndex = childIndex;
  } else if (childIndex > 0 &&
             static_cast<int>(parent->children[childIndex - 1]->entries.size()) < maxEntries()) {
    leftIndex = childIndex - 1;
  } else {
    return false;
  }

  Node* left = parent->children[leftIndex];
  Node* right = parent->children[leftIndex + 1];

  std::vector<Entry> entries = left->entries;
  entries.push_back(parent->entries[leftIndex]);
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int mid = static_cast<int>(entries.size()) / 2;

  left->entries.assign(entries.begin(), entries.begin() + mid);
  parent->entries[leftIndex] = entries[mid];
  right->entries.assign(entries.begin() + mid + 1, entries.end());

  if (!left->isLeaf) {
    std::vector<Node*> children = left->children;
    children.insert(children.end(), right->children.begin(), right->children.end());

    int leftChildCount = static_cast<int>(left->entries.size()) + 1;

    left->children.assign(children.begin(), children.begin() + leftChildCount);
    right->children.assign(children.begin() + leftChildCount, children.end());
  }

  return true;
}

void BStarTree::concatenation(Node*, int) {
  // TODO
}

void BStarTree::redistributeUnderflow(Node*, int) {
  // TODO
}

void BStarTree::handleUnderflow(Node*, std::vector<std::pair<Node*, int>>&) {
  // TODO
}
