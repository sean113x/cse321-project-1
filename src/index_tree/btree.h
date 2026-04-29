#pragma once

#include "index-tree.h"
#include <utility>
#include <vector>

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
    std::vector<Node *> children;

    explicit Node(bool isLeaf);
  };

  Node *root;

  // capacity helper function
  int minEntries() const override { return (order + 1) / 2 - 1; }

  // search() helper functions
  int findIndex(const std::vector<Entry> &entries, int key) const;
  int search(Node *node, int key) const;

  // range_query() helper functions
  void range_query(Node *node, int startKey, int endKey,
                   std::vector<int> &rids) const;
  void put_values(Node *node, int endKey, std::vector<int> &rids) const;

  // insert() helper functions
  Entry splitNode(Node *node, Node *&rightNode);
  void handleOverflow(Node *node, std::vector<std::pair<Node *, int>> &path);

  // remove() helper functions
  void concatenation(Node *parent, int leftIndex);
  void redistribution(Node *parent, int leftIndex);
  void handleUnderflow(Node *node, std::vector<std::pair<Node *, int>> &path);

public:
  explicit BTree(int order);
  ~BTree() override;

  int search(int key) const override;
  std::vector<int> range_query(int startKey, int endKey) const override;
  void insert(int key, int rid) override;
  void remove(int key) override;
};
