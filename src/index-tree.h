#pragma once

#include <vector>

class IndexTree {
protected:
  int order;
  int splitCount; // The number of split operations performed (for experiments).

  int maxEntries() const { return order - 1; }

  virtual int minEntries() const = 0;

public:
  explicit IndexTree(int order) : order(order), splitCount(0) {}

  virtual ~IndexTree() = default;

  virtual int search(int key) const = 0;
  virtual std::vector<int> search_range(int startKey, int endKey) const = 0;
  virtual void insert(int key, int rid) = 0;
  virtual void remove(int key) = 0;

  int getOrder() const { return order; }

  int getSplitCount() const { return splitCount; }
};