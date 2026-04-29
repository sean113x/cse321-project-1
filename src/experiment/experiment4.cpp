#include "experiment4.h"

#include "../dataset_handler/dataset.h"
#include "../index_tree/bplustree.h"
#include "../index_tree/bstartree.h"
#include "../index_tree/btree.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {
constexpr int warmupRuns = 10;
constexpr int minMeasuredRuns = 30;
constexpr int maxRuns = 200;
constexpr double targetRsd = 0.02;

struct TreeSpec {
  const char *name;
  int type;
};

struct WorkloadSpec {
  const char *name;
  int deleteCount;
};

struct TreeStats {
  int height = 0;
  int numNodes = 0;
  int numEntries = 0;
  double nodeUtilization = 0.0;
};

struct RunState {
  const TreeSpec *spec;
  std::vector<double> executionTimes;
  TreeStats before;
  TreeStats after;
  int foundAfter = 0;
  double avg = 0.0;
  double sd = 0.0;
  double rsd = 0.0;
  bool done = false;

  explicit RunState(const TreeSpec *spec) : spec(spec) {}
};

std::unique_ptr<IndexTree> createTree(int type, int order) {
  if (type == 1) {
    return std::make_unique<BTree>(order);
  }
  if (type == 2) {
    return std::make_unique<BStarTree>(order);
  }
  return std::make_unique<BPlusTree>(order);
}

double mean(const std::vector<double> &values) {
  double sum = 0.0;
  for (double value : values) {
    sum += value;
  }
  return sum / static_cast<double>(values.size());
}

double median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  int mid = static_cast<int>(values.size()) / 2;
  return values.size() % 2 == 1 ? values[mid]
                                : (values[mid - 1] + values[mid]) / 2.0;
}

double stddev(const std::vector<double> &values, double avg) {
  double sum = 0.0;
  for (double value : values) {
    double diff = value - avg;
    sum += diff * diff;
  }
  return values.size() < 2
             ? 0.0
             : std::sqrt(sum / static_cast<double>(values.size() - 1));
}

void createResultsDirectory() {
  if (mkdir("results_experiment", 0755) != 0 && errno != EEXIST) {
    throw std::runtime_error("Failed to create results_experiment directory");
  }
}

void buildTree(IndexTree &tree, const std::vector<int> &keys) {
  for (int rid = 0; rid < static_cast<int>(keys.size()); ++rid) {
    tree.insert(keys[rid], rid);
  }
}

TreeStats getStats(IndexTree &tree) {
  return {tree.getHeight(), tree.getNumNode(), tree.getNumEntry(),
          tree.getNodeUtilization()};
}

std::vector<int> makeDeleteKeys(const std::vector<int> &keys, int seed,
                                int deleteCount) {
  std::vector<int> indexes(keys.size());
  std::iota(indexes.begin(), indexes.end(), 0);
  std::mt19937 generator(seed);
  std::shuffle(indexes.begin(), indexes.end(), generator);

  std::vector<int> deleteKeys;
  deleteKeys.reserve(deleteCount);
  for (int i = 0; i < deleteCount; ++i) {
    deleteKeys.push_back(keys[indexes[i]]);
  }
  return deleteKeys;
}

long long deleteKeys(IndexTree &tree, const std::vector<int> &keys) {
  auto start = std::chrono::steady_clock::now();
  for (int key : keys) {
    tree.remove(key);
  }
  auto end = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
      .count();
}

int countFoundKeys(IndexTree &tree, const std::vector<int> &keys) {
  int found = 0;
  for (int key : keys) {
    if (tree.search(key) != -1) {
      found++;
    }
  }
  return found;
}
} // namespace

int runExperiment4() {
  const std::vector<TreeSpec> trees = {
      {"btree", 1}, {"bstar", 2}, {"bplus", 3}};
  const std::vector<int> orders = {3, 5, 10, 16, 32, 64, 128, 256, 512, 1024};

  Dataset dataset = loadDataset("data/student.csv");
  std::vector<int> keys;
  keys.reserve(dataset.size());
  for (int rid = 0; rid < dataset.size(); ++rid) {
    keys.push_back(dataset.getKey(rid));
  }

  const std::vector<WorkloadSpec> workloads = {
      {"delete_10_percent", static_cast<int>(keys.size()) / 10},
      {"delete_20_percent", static_cast<int>(keys.size()) / 5}};

  createResultsDirectory();
  std::ofstream runFile("results_experiment/experiment4_runs.csv");
  std::ofstream summaryFile("results_experiment/experiment4_summary.csv");
  runFile << std::setprecision(12);
  summaryFile << std::setprecision(12);

  runFile << "tree,order,workload,delete_count,seed,execution_time_ns,"
          << "before_height,after_height,before_num_nodes,after_num_nodes,"
          << "before_num_entries,after_num_entries,before_node_utilization,"
          << "after_node_utilization,found_after\n";
  summaryFile << "tree,order,workload,delete_count,records,warmup_runs,"
              << "measured_runs,mean_execution_time_ns,"
              << "median_execution_time_ns,stddev_execution_time_ns,rsd,"
              << "mean_execution_time_ms,median_execution_time_ms,"
              << "before_height,after_height,before_num_nodes,after_num_nodes,"
              << "before_num_entries,after_num_entries,"
              << "before_node_utilization,after_node_utilization,found_after\n";

  std::cout << "Experiment 4: Deletion & Structural Integrity\n";
  std::cout << "Records: " << keys.size() << "\n\n";

  for (const WorkloadSpec &workload : workloads) {
    std::cout << "Workload: " << workload.name
              << ", delete_count=" << workload.deleteCount << '\n'
              << std::flush;

    for (int order : orders) {
      for (int seed = 1; seed <= warmupRuns; ++seed) {
        std::vector<int> deleteSet =
            makeDeleteKeys(keys, seed, workload.deleteCount);

        for (const TreeSpec &spec : trees) {
          auto tree = createTree(spec.type, order);
          buildTree(*tree, keys);
          deleteKeys(*tree, deleteSet);
        }
      }

      std::vector<RunState> states;
      for (const TreeSpec &spec : trees) {
        states.emplace_back(&spec);
      }

      for (int seed = warmupRuns + 1; seed <= warmupRuns + maxRuns; ++seed) {
        std::vector<int> deleteSet =
            makeDeleteKeys(keys, seed, workload.deleteCount);
        bool allDone = true;

        for (RunState &state : states) {
          if (state.done) {
            continue;
          }

          auto tree = createTree(state.spec->type, order);
          buildTree(*tree, keys);
          state.before = getStats(*tree);

          long long ns = deleteKeys(*tree, deleteSet);
          state.after = getStats(*tree);
          state.foundAfter = countFoundKeys(*tree, deleteSet);
          state.executionTimes.push_back(static_cast<double>(ns));
          state.avg = mean(state.executionTimes);
          state.sd = stddev(state.executionTimes, state.avg);
          state.rsd = state.sd / state.avg;

          runFile << state.spec->name << ',' << order << ',' << workload.name
                  << ',' << workload.deleteCount << ',' << seed << ',' << ns
                  << ',' << state.before.height << ',' << state.after.height
                  << ',' << state.before.numNodes << ',' << state.after.numNodes
                  << ',' << state.before.numEntries << ','
                  << state.after.numEntries << ','
                  << state.before.nodeUtilization << ','
                  << state.after.nodeUtilization << ',' << state.foundAfter
                  << '\n';

          if (static_cast<int>(state.executionTimes.size()) >=
                  minMeasuredRuns &&
              state.rsd < targetRsd) {
            state.done = true;
          }
        }

        for (const RunState &state : states) {
          allDone = allDone && state.done;
        }
        if (allDone) {
          break;
        }
      }

      for (const RunState &state : states) {
        double med = median(state.executionTimes);
        double meanMs = state.avg / 1000000.0;
        double medianMs = med / 1000000.0;

        summaryFile << state.spec->name << ',' << order << ',' << workload.name
                    << ',' << workload.deleteCount << ',' << keys.size() << ','
                    << warmupRuns << ',' << state.executionTimes.size() << ','
                    << state.avg << ',' << med << ',' << state.sd << ','
                    << state.rsd << ',' << meanMs << ',' << medianMs << ','
                    << state.before.height << ',' << state.after.height << ','
                    << state.before.numNodes << ',' << state.after.numNodes
                    << ',' << state.before.numEntries << ','
                    << state.after.numEntries << ','
                    << state.before.nodeUtilization << ','
                    << state.after.nodeUtilization << ',' << state.foundAfter
                    << '\n';

        std::cout << state.spec->name << " order=" << order
                  << " workload=" << workload.name
                  << " runs=" << state.executionTimes.size()
                  << " median_ms=" << medianMs << " mean_ms=" << meanMs
                  << " rsd=" << state.rsd << " height=" << state.before.height
                  << "->" << state.after.height
                  << " nodes=" << state.before.numNodes << "->"
                  << state.after.numNodes
                  << " entries=" << state.before.numEntries << "->"
                  << state.after.numEntries
                  << " utilization=" << state.before.nodeUtilization << "->"
                  << state.after.nodeUtilization
                  << " found_after=" << state.foundAfter << '\n';
      }
    }

    std::cout << '\n';
  }

  std::cout << "\nSaved results_experiment/experiment4_runs.csv\n";
  std::cout << "Saved results_experiment/experiment4_summary.csv\n";
  return 0;
}
