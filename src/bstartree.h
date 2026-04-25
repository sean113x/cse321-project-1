#pragma once

#include "index-tree.h"
#include <vector>
#include <utility>

class BStarTree : public IndexTree {
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

  // capacity helper functions
  int minEntries() const override;

  // search() helper functions
  int findIndex(const std::vector<Entry>& entries, int key) const;
  int search(Node* node, int key) const;

  // insert() helper functions
  Entry splitNode(Node* node, Node*& rightNode);
  void handleOverflow(Node* node, std::vector<std::pair<Node*, int>>& path);
  bool redistributeForInsert(Node* parent, int childIndex);
  void splitIntoThree(Node* parent, int leftIndex);

  // remove() helper functions
  void handleUnderflow(Node* node, std::vector<std::pair<Node*, int>>& path);
  void concatenation(Node* parent, int leftIndex);
  void redistribution(Node* parent, int leftIndex);

public:
  explicit BStarTree(int order);
  ~BStarTree() override;

  int search(int key) const override;
  void insert(int key, int rid) override;
  void remove(int key) override;
};
