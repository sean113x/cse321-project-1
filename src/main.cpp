#include "dataset_handler/dataset.h"
#include "index_tree/bplustree.h"
#include "index_tree/bstartree.h"
#include "index_tree/btree.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

std::string prompt(const std::string &message,
                   const std::string &defaultValue) {
  std::cout << message << " [" << defaultValue << "]: ";

  std::string input;
  std::getline(std::cin, input);
  return input.empty() ? defaultValue : input;
}

std::unique_ptr<IndexTree> createTree(const std::string &type, int order) {
  if (type == "1" || type == "btree" || type == "b-tree") {
    return std::make_unique<BTree>(order);
  }
  if (type == "2" || type == "bstar" || type == "b*-tree") {
    return std::make_unique<BStarTree>(order);
  }
  if (type == "3" || type == "bplus" || type == "b+-tree") {
    return std::make_unique<BPlusTree>(order);
  }

  throw std::invalid_argument("Unknown tree type: " + type);
}

void printOperations(const std::string &keyHeader) {
  std::cout << "Operations:\n";
  std::cout << "  search <" << keyHeader << ">\n";
  std::cout << "  range <start> <end>\n";
  std::cout << "  delete <" << keyHeader << ">\n";
}

int runTestMode() {
  std::string datasetPath = prompt("Dataset path", "data/student.csv");
  std::string treeType = prompt("Tree type (1=btree, 2=bstar, 3=bplus)", "3");
  int order = std::stoi(prompt("Tree order", "4"));

  Dataset dataset = loadDataset(datasetPath);
  std::unique_ptr<IndexTree> tree = createTree(treeType, order);
  std::string keyHeader = dataset.getKeyHeader();

  auto start = std::chrono::steady_clock::now();
  for (int rid = 0; rid < dataset.size(); ++rid) {
    tree->insert(dataset.getKey(rid), rid);
  }
  auto end = std::chrono::steady_clock::now();
  auto buildMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();

  std::cout << "\nLoaded " << dataset.size() << " records\n";
  std::cout << "Key = " << keyHeader << ", RID = array index\n";
  std::cout << "Tree order = " << tree->getOrder() << '\n';
  std::cout << "Build time = " << buildMs << " ms\n";
  std::cout << "Split count = " << tree->getSplitCount() << '\n';
  std::cout << "Number of nodes = " << tree->getNumNode() << '\n';
  std::cout << "Number of entries = " << tree->getNumEntry() << '\n';
  std::cout << "Node utilization = " << std::fixed << std::setprecision(2)
            << tree->getNodeUtilization() << "%\n";
  std::cout << '\n';
  printOperations(keyHeader);

  std::string line;
  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) {
      break;
    }

    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == "range") {
      int startKey = 0;
      int endKey = 0;

      if (!(input >> startKey >> endKey)) {
        std::cout << "Usage: range <start> <end>\n";
        continue;
      }

      std::vector<int> rids = tree->range_query(startKey, endKey);

      if (rids.empty()) {
        std::cout << "No records found\n";
        continue;
      }

      for (int rid : rids) {
        std::cout << dataset.getRecordString(rid) << '\n';
      }

      continue;
    }
    if (command != "search" && command != "delete") {
      std::cout << "Unknown command.\n";
      continue;
    }

    int key = 0;
    if (!(input >> key)) {
      std::cout << "Usage: " << command << " <" << keyHeader << ">\n";
      continue;
    }

    int rid = tree->search(key);
    if (rid == -1) {
      std::cout << keyHeader << " " << key << " not found\n";
      continue;
    }

    if (command == "search") {
      std::cout << dataset.getRecordString(rid) << '\n';
    } else {
      tree->remove(key);
      std::cout << "Deleted " << dataset.getRecordString(rid) << '\n';
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {
  try {
    if (argc < 2) {
      return runTestMode();
    }

    std::string mode = argv[1];
    if (mode == "experiment") {
      std::cout << "Experiment mode is not implemented yet.\n";
      return 0;
    }

    std::cout << "Usage: ./project1 [experiment]\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
