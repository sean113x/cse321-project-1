#include "experiment1.h"

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

struct Measurement {
  double executionTimeMs = 0.0;
  long long nodeReadCount = 0;
  long long sequentialLeafReadCount = 0;
  double simulatedSsdCostMs = 0.0;
  double totalTimeWithSsdMs = 0.0;
};

struct RunState {
  const TreeSpec *spec;
  std::vector<double> executionTimes;
  std::vector<double> simulatedSsdCosts;
  std::vector<double> totalTimesWithSsd;
  std::unique_ptr<IndexTree> tree;
  long long nodeReadCount = 0;
  long long sequentialLeafReadCount = 0;
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

  if (values.size() % 2 == 1) {
    return values[mid];
  }

  return (values[mid - 1] + values[mid]) / 2.0;
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

Measurement buildTree(IndexTree &tree, const std::vector<int> &keys) {
  tree.resetNodeReadCount();
  auto start = std::chrono::steady_clock::now(); // start timer
  for (int rid = 0; rid < static_cast<int>(keys.size()); ++rid) {
    tree.insert(keys[rid], rid);
  }
  auto end = std::chrono::steady_clock::now(); // end timer
  Measurement measurement;
  measurement.executionTimeMs =
      std::chrono::duration<double, std::milli>(end - start).count();
  measurement.nodeReadCount = tree.getNodeReadCount();
  measurement.sequentialLeafReadCount = tree.getSequentialLeafReadCount();
  measurement.simulatedSsdCostMs = tree.getSimulatedSsdCostMs();
  measurement.totalTimeWithSsdMs =
      measurement.executionTimeMs + measurement.simulatedSsdCostMs;
  return measurement;
}

void createResultsDirectory() {
  if (mkdir("results_experiment", 0755) != 0 && errno != EEXIST) {
    throw std::runtime_error("Failed to create results_experiment directory");
  }
}
} // namespace

int runExperiment1() {
  const std::vector<TreeSpec> trees = {
      {"btree", 1}, {"bstar", 2}, {"bplus", 3}};
  const std::vector<int> orders = {3, 5, 10, 16, 32, 64, 128, 256, 512, 1024};

  Dataset dataset = loadDataset("data/student.csv");
  std::vector<int> keys;
  keys.reserve(dataset.size());
  for (int rid = 0; rid < dataset.size(); ++rid) {
    keys.push_back(dataset.getKey(rid));
  }

  createResultsDirectory();
  std::ofstream runFile("results_experiment/experiment1_runs.csv");
  std::ofstream summaryFile("results_experiment/experiment1_summary.csv");
  runFile << std::setprecision(12);
  summaryFile << std::setprecision(12);

  runFile << "tree,order,run,execution_time_ms,node_read_count,"
          << "sequential_leaf_read_count,simulated_ssd_cost_ms,"
          << "total_time_with_ssd_ms\n";
  summaryFile << "tree,order,records,warmup_runs,measured_runs,"
              << "mean_execution_time_ms,median_execution_time_ms,"
              << "stddev_execution_time_ms,rsd,node_read_count,"
              << "sequential_leaf_read_count,"
              << "mean_simulated_ssd_cost_ms,median_simulated_ssd_cost_ms,"
              << "mean_total_time_with_ssd_ms,median_total_time_with_ssd_ms,"
              << "split_count,tree_height,"
              << "num_nodes,num_entries,node_utilization\n";

  std::cout << "Experiment 1: Insertion & Parameter Tuning\n";
  std::cout << "Records: " << keys.size() << "\n\n";

  for (int order : orders) {
    for (int run = 0; run < warmupRuns; ++run) {
      for (const TreeSpec &spec : trees) {
        auto tree = createTree(spec.type, order);
        buildTree(*tree, keys);
      }
    }

    std::vector<RunState> states;
    for (const TreeSpec &spec : trees) {
      states.emplace_back(&spec);
    }

    for (int run = 1; run <= maxRuns; ++run) {
      bool allDone = true;

      for (RunState &state : states) {
        if (state.done) {
          continue;
        }

        state.tree = createTree(state.spec->type, order);
        Measurement measurement = buildTree(*state.tree, keys);
        state.executionTimes.push_back(measurement.executionTimeMs);
        state.nodeReadCount = measurement.nodeReadCount;
        state.sequentialLeafReadCount = measurement.sequentialLeafReadCount;
        state.simulatedSsdCosts.push_back(measurement.simulatedSsdCostMs);
        state.totalTimesWithSsd.push_back(measurement.totalTimeWithSsdMs);
        state.avg = mean(state.executionTimes);
        state.sd = stddev(state.executionTimes, state.avg);
        state.rsd = state.sd / state.avg;

        runFile << state.spec->name << ',' << order << ',' << run << ','
                << measurement.executionTimeMs << ','
                << measurement.nodeReadCount << ','
                << measurement.sequentialLeafReadCount << ','
                << measurement.simulatedSsdCostMs << ','
                << measurement.totalTimeWithSsdMs << '\n';

        if (run >= minMeasuredRuns && state.rsd < targetRsd) {
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
      double meanSsdCost = mean(state.simulatedSsdCosts);
      double medianSsdCost = median(state.simulatedSsdCosts);
      double meanTotalWithSsd = mean(state.totalTimesWithSsd);
      double medianTotalWithSsd = median(state.totalTimesWithSsd);

      summaryFile << state.spec->name << ',' << order << ',' << keys.size()
                  << ',' << warmupRuns << ',' << state.executionTimes.size()
                  << ',' << state.avg << ',' << med << ',' << state.sd << ','
                  << state.rsd << ',' << state.nodeReadCount << ','
                  << state.sequentialLeafReadCount << ',' << meanSsdCost << ','
                  << medianSsdCost << ','
                  << meanTotalWithSsd << ',' << medianTotalWithSsd << ','
                  << state.tree->getSplitCount() << ','
                  << state.tree->getHeight() << ',' << state.tree->getNumNode()
                  << ',' << state.tree->getNumEntry() << ','
                  << state.tree->getNodeUtilization() << '\n';

      std::cout << state.spec->name << " order=" << order
                << " runs=" << state.executionTimes.size()
                << " median_ms=" << med << " mean_ms=" << state.avg
                << " node_reads=" << state.nodeReadCount
                << " sequential_leaf_reads=" << state.sequentialLeafReadCount
                << " ssd_median_ms=" << medianSsdCost
                << " ssd_mean_ms=" << meanSsdCost
                << " total_with_ssd_median_ms=" << medianTotalWithSsd
                << " total_with_ssd_mean_ms=" << meanTotalWithSsd
                << " rsd=" << state.rsd
                << " splits=" << state.tree->getSplitCount()
                << " height=" << state.tree->getHeight()
                << " utilization=" << state.tree->getNodeUtilization() << "%\n";
    }
  }

  std::cout << "\nSaved results_experiment/experiment1_runs.csv\n";
  std::cout << "Saved results_experiment/experiment1_summary.csv\n";
  return 0;
}
