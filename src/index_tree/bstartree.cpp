#include "bstartree.h"

#include <stdexcept>

BStarTree::Entry::Entry(int key, int rid) : key(key), rid(rid) {}

BStarTree::Node::Node(bool isLeaf) : isLeaf(isLeaf) {}

BStarTree::BStarTree(int order) : IndexTree(order), root(nullptr) {
  if (order < 3) {
    throw std::invalid_argument("BStarTree order must be at least 3.");
  }
}

BStarTree::~BStarTree() {
  auto clear = [](auto &&self, Node *node) -> void {
    if (node == nullptr) {
      return;
    }
    for (Node *child : node->children) {
      self(self, child);
    }
    delete node;
  };
  clear(clear, root);
}

/*
  Main functions: (same as B-tree)
  - search(): retrieve the key-rid pair.
  - search_range(): retrieve the key-rid pairs in the range.
  - insert(): insert the key-rid pair into leaf node.
  - remove(): remove the key-rid pair.
  - getNodeUtilization(): calculate average node utilization.
*/

int BStarTree::search(int key) const { return search(root, key); }

std::vector<int> BStarTree::search_range(int startKey, int endKey) const {
  std::vector<int> rids;

  if (startKey > endKey) {
    return rids;
  }

  search_range(root, startKey, endKey, rids);
  return rids;
}

void BStarTree::insert(int key, int rid) {
  Entry newEntry(key, rid);

  if (root == nullptr) {
    root = new Node(true);
    root->entries.push_back(newEntry);
    numNode++;
    numEntry++;
    return;
  }

  std::vector<std::pair<Node *, int>> path;
  Node *current = root;

  while (!current->isLeaf) {
    int index = findIndex(current->entries, key);

    // If the key already exists in an internal node, do not insert
    if (index < static_cast<int>(current->entries.size()) &&
        current->entries[index].key == key) {
      return;
    }
    path.push_back({current, index});
    current = current->children[index];
  }

  int index = findIndex(current->entries, key);

  // If the key already exists in a leaf node, do not insert
  if (index < static_cast<int>(current->entries.size()) &&
      current->entries[index].key == key) {
    return;
  }

  current->entries.insert(current->entries.begin() + index, newEntry);
  numEntry++;
  handleOverflow(current, path);
}

void BStarTree::remove(int key) {
  if (root == nullptr) {
    return;
  }

  std::vector<std::pair<Node *, int>> path;
  Node *current = root;

  while (true) {
    int index = findIndex(current->entries, key);

    if (index < static_cast<int>(current->entries.size()) &&
        current->entries[index].key == key) {
      if (!current->isLeaf) {
        Node *successor = current->children[index + 1];
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
      numEntry--;
      handleUnderflow(current, path);
      return;
    }

    if (current->isLeaf) {
      return;
    }

    path.push_back({current, index});
    current = current->children[index];
  }
}

double BStarTree::getNodeUtilization() const {
  if (numNode == 0) {
    return 0.0;
  }

  int totalCapacity = rootMaxEntries() + (numNode - 1) * maxEntries();
  return 100.0 * numEntry / totalCapacity;
}

/*
  Helper functions:
  - findIndex(): find the first key that is >= the given key.
  - search(): recursively perform search.
  - search_range(): recursively perform in-order range search.
  - put_values(): put values in the subtree that is <= endKey.
  - splitNode(): split two full sibling nodes into three nodes (2-to-3 split).
  - redistributeOverflow(): redistribute entries with a sibling before
  splitting.
  - handleOverflow(): handle the overflow by splitting node.
  - concatenation(): merge the child and its right sibling.
  - redistribution2(): redistribute entries between two neighboring children.
  - redistribution3(): redistribute entries among three neighboring children.
  - handleUnderflow(): handle the underflow by concatenation or redistributions.
*/

int BStarTree::findIndex(const std::vector<Entry> &entries,
                         int key) const { // same as B-tree
  int index = 0;

  while (index < static_cast<int>(entries.size()) && entries[index].key < key) {
    ++index;
  }

  return index;
}

int BStarTree::search(Node *node, int key) const { // same as B-tree
  if (node == nullptr) {
    return -1;
  }
  int index = findIndex(node->entries, key);

  if (index < static_cast<int>(node->entries.size()) &&
      node->entries[index].key == key) {
    return node->entries[index].rid;
  }

  if (node->isLeaf) {
    return -1;
  }

  return search(node->children[index], key);
}

void BStarTree::search_range(Node *node, int startKey, int endKey,
                             std::vector<int> &rids) const {
  if (node == nullptr) {
    return;
  }

  int index = findIndex(node->entries, startKey);

  if (!node->isLeaf) {
    search_range(node->children[index], startKey, endKey, rids);
  }

  while (index < static_cast<int>(node->entries.size()) &&
         node->entries[index].key <= endKey) {
    rids.push_back(node->entries[index].rid);
    index++;

    if (!node->isLeaf) {
      put_values(node->children[index], endKey, rids);
    }
  }
}

void BStarTree::put_values(Node *node, int endKey,
                           std::vector<int> &rids) const {
  if (node == nullptr) {
    return;
  }

  for (int i = 0; i < static_cast<int>(node->entries.size()); i++) {
    if (!node->isLeaf) {
      put_values(node->children[i], endKey, rids);
    }

    if (node->entries[i].key > endKey) {
      return;
    }

    rids.push_back(node->entries[i].rid);
  }

  if (!node->isLeaf) {
    put_values(node->children.back(), endKey, rids);
  }
}

void BStarTree::splitNode(Node *parent, int leftIndex) {
  Node *left = parent->children[leftIndex];
  Node *right = parent->children[leftIndex + 1];
  Node *middle = new Node(left->isLeaf);
  numNode++;

  std::vector<Entry> entries = left->entries;
  entries.push_back(
      parent->entries[leftIndex]); // Add the separating entry from the parent.
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int entryCount = static_cast<int>(entries.size()) - 2; // 2 separator.
  int firstUpIndex = (entryCount + 2) / 3;
  int secondUpIndex = firstUpIndex + 1 + (entryCount + 1) / 3;

  // Distribute entries to left, middle, and right nodes. Promote two separators
  // to the parent node.
  left->entries.assign(entries.begin(), entries.begin() + firstUpIndex);
  parent->entries[leftIndex] = entries[firstUpIndex];
  middle->entries.assign(entries.begin() + firstUpIndex + 1,
                         entries.begin() + secondUpIndex);
  parent->entries.insert(parent->entries.begin() + leftIndex + 1,
                         entries[secondUpIndex]);
  right->entries.assign(entries.begin() + secondUpIndex + 1, entries.end());

  if (!left->isLeaf) { // Internal node case: distribute child pointers as well.
    std::vector<Node *> children = left->children;
    children.insert(children.end(), right->children.begin(),
                    right->children.end());

    int firstChildEnd = firstUpIndex + 1;
    int secondChildEnd = secondUpIndex + 1;

    left->children.assign(children.begin(), children.begin() + firstChildEnd);
    middle->children.assign(children.begin() + firstChildEnd,
                            children.begin() + secondChildEnd);
    right->children.assign(children.begin() + secondChildEnd, children.end());
  }

  parent->children.insert(parent->children.begin() + leftIndex + 1, middle);
  splitCount++;
}

void BStarTree::handleOverflow(Node *node,
                               std::vector<std::pair<Node *, int>> &path) {
  while (static_cast<int>(node->entries.size()) >
         (node == root ? rootMaxEntries() : maxEntries())) {
    if (path.empty()) { // If the root node overflows, split it into two nodes.
      int mid = static_cast<int>(node->entries.size()) / 2;
      Entry upEntry = node->entries[mid];
      Node *rightNode = new Node(node->isLeaf);
      numNode++;

      rightNode->entries.assign(node->entries.begin() + mid + 1,
                                node->entries.end());
      node->entries.erase(node->entries.begin() + mid, node->entries.end());

      if (!node->isLeaf) {
        rightNode->children.assign(node->children.begin() + mid + 1,
                                   node->children.end());
        node->children.resize(mid + 1);
      }

      root = new Node(false);
      numNode++;
      root->entries = {upEntry};
      root->children = {node, rightNode};

      splitCount++;
      return;
    }

    auto [parent, childIndex] = path.back();
    path.pop_back();

    // Try to redistribution first.
    if (redistributeOverflow(parent, childIndex)) {
      return;
    }

    int leftIndex = (childIndex < static_cast<int>(parent->children.size()) - 1)
                        ? childIndex
                        : childIndex - 1;

    splitNode(parent, leftIndex);
    node = parent;
  }
}

bool BStarTree::redistributeOverflow(Node *parent, int childIndex) {
  int leftIndex = -1;

  if (childIndex < static_cast<int>(parent->children.size()) - 1 &&
      static_cast<int>(parent->children[childIndex + 1]->entries.size()) <
          maxEntries()) {
    leftIndex = childIndex;
  } else if (childIndex > 0 &&
             static_cast<int>(
                 parent->children[childIndex - 1]->entries.size()) <
                 maxEntries()) {
    leftIndex = childIndex - 1;
  } else {
    return false;
  }

  Node *left = parent->children[leftIndex];
  Node *right = parent->children[leftIndex + 1];

  std::vector<Entry> entries = left->entries;
  entries.push_back(parent->entries[leftIndex]);
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int mid = static_cast<int>(entries.size()) / 2;

  left->entries.assign(entries.begin(), entries.begin() + mid);
  parent->entries[leftIndex] = entries[mid];
  right->entries.assign(entries.begin() + mid + 1, entries.end());

  if (!left->isLeaf) {
    std::vector<Node *> children = left->children;
    children.insert(children.end(), right->children.begin(),
                    right->children.end());

    int leftChildCount = static_cast<int>(left->entries.size()) + 1;

    left->children.assign(children.begin(), children.begin() + leftChildCount);
    right->children.assign(children.begin() + leftChildCount, children.end());
  }

  return true;
}

void BStarTree::concatenation(Node *parent, int childIndex) {
  int startIndex = -1;
  int childCount = static_cast<int>(parent->children.size());

  for (int candidate : {childIndex - 2, childIndex - 1, childIndex}) {
    if (candidate < 0 || candidate + 2 >= childCount) {
      continue;
    }

    Node *first = parent->children[candidate];
    Node *second = parent->children[candidate + 1];
    Node *third = parent->children[candidate + 2];

    int total = static_cast<int>(first->entries.size()) +
                static_cast<int>(second->entries.size()) +
                static_cast<int>(third->entries.size()) + 2;

    int childEntryCount = total - 1;
    if (childEntryCount >= 2 * minEntries() &&
        childEntryCount <= 2 * maxEntries()) {
      startIndex = candidate;
      break;
    }
  }

  if (startIndex == -1) {
    int leftIndex = (childIndex > 0) ? childIndex - 1 : childIndex;
    Node *left = parent->children[leftIndex];
    Node *right = parent->children[leftIndex + 1];

    left->entries.push_back(parent->entries[leftIndex]);
    left->entries.insert(left->entries.end(), right->entries.begin(),
                         right->entries.end());

    if (!left->isLeaf) {
      left->children.insert(left->children.end(), right->children.begin(),
                            right->children.end());
    }

    parent->entries.erase(parent->entries.begin() + leftIndex);
    parent->children.erase(parent->children.begin() + leftIndex + 1);
    delete right;
    numNode--;
    return;
  }

  Node *first = parent->children[startIndex];
  Node *second = parent->children[startIndex + 1];
  Node *third = parent->children[startIndex + 2];

  std::vector<Entry> entries = first->entries;
  entries.push_back(parent->entries[startIndex]);
  entries.insert(entries.end(), second->entries.begin(), second->entries.end());
  entries.push_back(parent->entries[startIndex + 1]);
  entries.insert(entries.end(), third->entries.begin(), third->entries.end());

  int childEntryCount = static_cast<int>(entries.size()) - 1;
  int leftEntryCount = (childEntryCount + 1) / 2;
  if (leftEntryCount > maxEntries()) {
    leftEntryCount = maxEntries();
  }
  if (childEntryCount - leftEntryCount < minEntries()) {
    leftEntryCount = childEntryCount - minEntries();
  }

  first->entries.assign(entries.begin(), entries.begin() + leftEntryCount);
  parent->entries[startIndex] = entries[leftEntryCount];
  second->entries.assign(entries.begin() + leftEntryCount + 1, entries.end());

  if (!first->isLeaf) {
    std::vector<Node *> children = first->children;
    children.insert(children.end(), second->children.begin(),
                    second->children.end());
    children.insert(children.end(), third->children.begin(),
                    third->children.end());

    int firstChildCount = static_cast<int>(first->entries.size()) + 1;

    first->children.assign(children.begin(),
                           children.begin() + firstChildCount);
    second->children.assign(children.begin() + firstChildCount, children.end());
  }

  parent->entries.erase(parent->entries.begin() + startIndex + 1);
  parent->children.erase(parent->children.begin() + startIndex + 2);
  delete third;
  numNode--;
}

bool BStarTree::redistribution2(Node *parent, int childIndex) {
  int leftIndex = -1;

  for (int candidate : {childIndex - 1, childIndex}) {
    if (candidate < 0 ||
        candidate + 1 >= static_cast<int>(parent->children.size())) {
      continue;
    }

    Node *left = parent->children[candidate];
    Node *right = parent->children[candidate + 1];
    int total = static_cast<int>(left->entries.size()) + 1 +
                static_cast<int>(right->entries.size());

    if (total >= 2 * minEntries() + 1) {
      leftIndex = candidate;
      break;
    }
  }

  if (leftIndex == -1) {
    return false;
  }

  Node *left = parent->children[leftIndex];
  Node *right = parent->children[leftIndex + 1];

  std::vector<Entry> entries = left->entries;
  entries.push_back(parent->entries[leftIndex]);
  entries.insert(entries.end(), right->entries.begin(), right->entries.end());

  int mid = static_cast<int>(entries.size()) / 2;

  left->entries.assign(entries.begin(), entries.begin() + mid);
  parent->entries[leftIndex] = entries[mid];
  right->entries.assign(entries.begin() + mid + 1, entries.end());

  if (!left->isLeaf) {
    std::vector<Node *> children = left->children;
    children.insert(children.end(), right->children.begin(),
                    right->children.end());

    int leftChildCount = static_cast<int>(left->entries.size()) + 1;

    left->children.assign(children.begin(), children.begin() + leftChildCount);
    right->children.assign(children.begin() + leftChildCount, children.end());
  }

  return true;
}

bool BStarTree::redistribution3(Node *parent, int childIndex) {
  int startIndex = -1;
  int childCount = static_cast<int>(parent->children.size());

  for (int candidate : {childIndex - 2, childIndex - 1, childIndex}) {
    if (candidate < 0 || candidate + 2 >= childCount) {
      continue;
    }

    Node *first = parent->children[candidate];
    Node *second = parent->children[candidate + 1];
    Node *third = parent->children[candidate + 2];

    int total = static_cast<int>(first->entries.size()) +
                static_cast<int>(second->entries.size()) +
                static_cast<int>(third->entries.size()) + 2;

    int childEntryCount = total - 2;
    if (childEntryCount >= 3 * minEntries() &&
        childEntryCount <= 3 * maxEntries()) {
      startIndex = candidate;
      break;
    }
  }

  if (startIndex == -1) {
    return false;
  }

  Node *first = parent->children[startIndex];
  Node *second = parent->children[startIndex + 1];
  Node *third = parent->children[startIndex + 2];

  std::vector<Entry> entries = first->entries;
  entries.push_back(parent->entries[startIndex]);
  entries.insert(entries.end(), second->entries.begin(), second->entries.end());
  entries.push_back(parent->entries[startIndex + 1]);
  entries.insert(entries.end(), third->entries.begin(), third->entries.end());

  int childEntryCount = static_cast<int>(entries.size()) - 2;
  int firstEntryCount = (childEntryCount + 2) / 3;
  int secondEntryCount = (childEntryCount + 1) / 3;
  int firstUpIndex = firstEntryCount;
  int secondUpIndex = firstUpIndex + 1 + secondEntryCount;

  first->entries.assign(entries.begin(), entries.begin() + firstUpIndex);
  parent->entries[startIndex] = entries[firstUpIndex];
  second->entries.assign(entries.begin() + firstUpIndex + 1,
                         entries.begin() + secondUpIndex);
  parent->entries[startIndex + 1] = entries[secondUpIndex];
  third->entries.assign(entries.begin() + secondUpIndex + 1, entries.end());

  if (!first->isLeaf) {
    std::vector<Node *> children = first->children;
    children.insert(children.end(), second->children.begin(),
                    second->children.end());
    children.insert(children.end(), third->children.begin(),
                    third->children.end());

    int firstChildEnd = firstUpIndex + 1;
    int secondChildEnd = secondUpIndex + 1;

    first->children.assign(children.begin(), children.begin() + firstChildEnd);
    second->children.assign(children.begin() + firstChildEnd,
                            children.begin() + secondChildEnd);
    third->children.assign(children.begin() + secondChildEnd, children.end());
  }

  return true;
}

void BStarTree::handleUnderflow(Node *node,
                                std::vector<std::pair<Node *, int>> &path) {
  while (node != root &&
         static_cast<int>(node->entries.size()) < minEntries()) {
    auto [parent, childIndex] = path.back();
    path.pop_back();

    if (redistribution2(parent, childIndex)) {
      return;
    }
    if (redistribution3(parent, childIndex)) {
      return;
    }

    concatenation(parent, childIndex);
    node = parent;
  }

  if (root != nullptr && root->entries.empty()) {
    Node *oldRoot = root;

    if (root->isLeaf) {
      root = nullptr;
    } else {
      root = root->children[0];
    }

    delete oldRoot;
    numNode--;
  }
}
