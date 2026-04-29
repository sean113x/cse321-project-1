#pragma once

#include <vector>

class IndexTree {
protected:
  int order;
  int splitCount; // The number of split operations performed (for experiments).
  int numNode;
  int numEntry;

  int maxEntries() const { return order - 1; }

  virtual int minEntries() const = 0;
  virtual int calculateHeight() const = 0;

public:
  explicit IndexTree(int order)
      : order(order), splitCount(0), numNode(0), numEntry(0) {}

  virtual ~IndexTree() = default;

  virtual int search(int key) const = 0;
  virtual std::vector<int> range_query(int startKey, int endKey) const = 0;
  virtual void insert(int key, int rid) = 0;
  virtual void remove(int key) = 0;

  int getOrder() const { return order; }

  int getSplitCount() const { return splitCount; }

  int getNumNode() const { return numNode; }

  int getNumEntry() const { return numEntry; }

  int getHeight() const { return calculateHeight(); }

  virtual double getNodeUtilization() const {
    if (numNode == 0) {
      return 0.0;
    }

    return 100.0 * numEntry / (numNode * maxEntries());
  }
};
