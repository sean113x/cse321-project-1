#pragma once

#include "index-tree.h"
#include <utility>
#include <vector>

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
    std::vector<Node *> children;

    explicit Node(bool isLeaf);
  };

  Node *root;

  // capacity helper function
  int minEntries() const override { return (2 * maxEntries()) / 3; }

  int rootMaxEntries() const { return 2 * minEntries(); }

  // search() helper functions
  int findIndex(const std::vector<Entry> &entries, int key) const;
  int search(Node *node, int key) const;

  // insert() helper functions
  void splitNode(Node *parent, int leftIndex);
  bool redistributeOverflow(Node *parent, int childIndex);
  void handleOverflow(Node *node, std::vector<std::pair<Node *, int>> &path);

  // remove() helper functions
  void concatenation(Node *parent, int childIndex);
  bool redistribution2(Node *parent, int childIndex);
  bool redistribution3(Node *parent, int childIndex);
  void handleUnderflow(Node *node, std::vector<std::pair<Node *, int>> &path);

public:
  explicit BStarTree(int order);
  ~BStarTree() override;

  int search(int key) const override;
  std::vector<int> search_range(int startKey, int endKey) const override;
  void insert(int key, int rid) override;
  void remove(int key) override;
};
