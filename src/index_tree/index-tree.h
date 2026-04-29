#pragma once

#include <vector>

class IndexTree {
protected:
  int order;
  int splitCount; // The number of split operations performed (for experiments).
  int numNode;
  int numEntry;
  mutable long long nodeReadCount;
  mutable long long sequentialLeafReadCount;

  int maxEntries() const { return order - 1; }

  void countNodeRead() const { nodeReadCount++; }
  void countSequentialLeafRead() const { sequentialLeafReadCount++; }

  virtual int minEntries() const = 0;
  virtual int calculateHeight() const = 0;

public:
  static constexpr double SSD_NODE_READ_COST_MS = 0.02;
  static constexpr double SSD_SEQUENTIAL_LEAF_READ_COST_MS = 0.004;

  explicit IndexTree(int order)
      : order(order), splitCount(0), numNode(0), numEntry(0),
        nodeReadCount(0), sequentialLeafReadCount(0) {}

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

  long long getNodeReadCount() const { return nodeReadCount; }

  long long getSequentialLeafReadCount() const {
    return sequentialLeafReadCount;
  }

  double getSimulatedSsdCostMs() const {
    return nodeReadCount * SSD_NODE_READ_COST_MS +
           sequentialLeafReadCount * SSD_SEQUENTIAL_LEAF_READ_COST_MS;
  }

  void resetNodeReadCount() const {
    nodeReadCount = 0;
    sequentialLeafReadCount = 0;
  }

  virtual double getNodeUtilization() const {
    if (numNode == 0) {
      return 0.0;
    }

    return 100.0 * numEntry / (numNode * maxEntries());
  }
};
