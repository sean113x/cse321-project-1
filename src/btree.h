#pragma once

#include "index-tree.h"
#include <vector>
#include <utility>

class BTree : public IndexTree {
private:
  struct Entry {
    int key;
    int rid;
    Entry(int key, int rid);
  };

  struct Node {
    bool isLeaf;
    std::vector<Entry> entries;
    std::vector<Node*> children;

    explicit Node(bool isLeaf);
  };

  Node* root;

  // capacity helper function
  int minEntries() const override {
    return (order + 1) / 2 - 1;
  }

  // search() helper functions
  int findIndex(const std::vector<Entry>& entries, int key) const;
  int search(Node* node, int key) const;

  // insert() helper functions
  Entry splitNode(Node* node, Node*& rightNode);
  void handleOverflow(Node* node, std::vector<std::pair<Node*, int>>& path);

  // remove() helper functions
  void concatenation(Node* parent, int leftIndex);
  void redistribution(Node* parent, int leftIndex);
  void handleUnderflow(Node* node, std::vector<std::pair<Node*, int>>& path);

public:
  explicit BTree(int order);
  ~BTree() override;

  int search(int key) const override;
  void insert(int key, int rid) override;
  void remove(int key) override;
};
