#pragma once

#include "index-tree.h"
#include <utility>
#include <vector>

class BPlusTree : public IndexTree {
private:
  struct Entry {
    int key;
    int rid;
    Entry(int key, int rid);
  };

  struct Node {
    bool isLeaf;

    explicit Node(bool isLeaf);
    virtual ~Node() = default;
  };

  struct InternalNode : Node {
    std::vector<int> keys;
    std::vector<Node *> children;

    InternalNode();
  };

  struct LeafNode : Node {
    std::vector<Entry> entries;
    LeafNode *next;

    LeafNode();
  };

  Node *root;

  // capacity helper function
  int minEntries() const override { return (order + 1) / 2 - 1; }

  // search() helper functions
  int findIndex(const std::vector<int> &keys, int key) const;
  int findIndex(const std::vector<Entry> &entries, int key) const;

  // search_range() helper functions
  std::pair<LeafNode *, int> search(Node *node, int key) const;

  // insert() helper functions
  int splitNode(Node *node, Node *&rightNode);
  void handleOverflow(Node *node,
                      std::vector<std::pair<InternalNode *, int>> &path);

  // remove() helper functions
  void concatenation(InternalNode *parent, int leftIndex);
  void redistribution(InternalNode *parent, int leftIndex);
  void handleUnderflow(Node *node,
                       std::vector<std::pair<InternalNode *, int>> &path);

public:
  explicit BPlusTree(int order);
  ~BPlusTree() override;

  int search(int key) const override;
  std::vector<int> search_range(int startKey, int endKey) const override;
  void insert(int key, int rid) override;
  void remove(int key) override;
};
