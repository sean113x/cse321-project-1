# CSE321 Project 1

> 🏆 Selected as a [Best Report](https://sites.google.com/view/junghoon-kim/databases) for CSE321 Individual Assignment #1: B-Tree Index Structures.

This project implements and benchmarks index tree structures in C++.

The project consists of the following main components:

1. B-Tree, B\*-Tree, and B+tree index implementations
2. Interactive index testing with search, range query, and delete operations
3. Reproducible benchmark experiments with CSV result exports

## Prerequisites

- A C++ compiler with C++20 support
- Tested successfully on a MacBook Pro with an Apple M1 Pro chip. If you need to borrow this machine for building or testing, contact me at **sean113x@unist.ac.kr**.

## Orientation

The implemented index trees share a common `IndexTree` interface:

- `BTree`: stores keys and record IDs in regular B-Tree nodes
- `BStarTree`: uses B\*-Tree-style redistribution and 2-to-3 split logic to improve node utilization
- `BPlusTree`: stores data entries in linked leaf nodes and keeps separator keys in internal nodes, following the B+tree structure

The default dataset is `data/student.csv`, which contains 100,000 student records plus a header row. The first column, `Student ID`, is used as the index key, and each row position is used as its record ID (`RID`).

## Installation

First, clone this repository with and move into the project directory.

```bash
git clone https://github.com/sean113x/cse321_database_systems.git
cd cse321_database_systems
```

#### Build from source

```bash
make
```

This compiles the project into the `project1` executable.

You can also clean build artifacts by `make clean`.

## Usage

The program has two modes:

- Interactive test mode: `./project1`
- Experiment mode: `./project1 experiment`

#### 01 Interactive test mode

Run the executable without arguments:

```bash
./project1
```

The program prompts for:

- Dataset path, default `data/student.csv`
- Tree type, where `1=btree`, `2=bstar`, and `3=bplus`; default `3`
- Tree order, default `4`

After the index is built, the following commands are available:

```text
search <Student ID>
range <start> <end>
delete <Student ID>
```

Example:

```text
Dataset path [data/student.csv]:
Tree type (1=btree, 2=bstar, 3=bplus) [3]:
Tree order [4]:
> search 202038411
> range 202000000 202100000
> delete 202038411
```

#### 02 Experiment mode

Run:

```bash
./project1 experiment
```

Then choose one of the built-in experiments:

```text
1. Insertion & Parameter Tuning
2. Point Search Performance
3. Range Query Performance
4. Deletion Performance
5. Intra-node Search Optimization
6. Selective Redistribution in B*-tree
```

Experiments 1 through 4 evaluate B-Tree, B\*-Tree, and B+tree over the following tree orders:

```text
3, 5, 10, 16, 32, 64, 128, 256, 512, 1024
```

Experiments 5 and 6 use specialized optimized B\*-Tree variants and order sets described below.

## Inputs

The main input is a comma-separated student dataset:

```text
Student ID,Name,Gender,GPA,Height,Weight
```

The dataset loader uses the first column as the key and converts it to an integer. The included dataset is:

```text
data/student.csv
```

## Outputs

Interactive mode prints matching records directly to the terminal.

Experiment mode writes result files to `results_experiment/`:

- `experiment<n>_runs.csv`
- `experiment<n>_summary.csv`

The result files include execution time, node-read counts, simulated SSD cost, total time including simulated SSD cost, tree height, node count, entry count, and node utilization where applicable.

## Experiments

> [!WARNING]
> Experiments can take some time because each tree type and tree order is measured repeatedly. The code runs 10 warm-up runs and then 30 to 200 measured runs, stopping early only when the relative standard deviation (`rsd`) is below `0.02`.

#### Experiment 1: Insertion & Parameter Tuning

Builds each tree from the full dataset and compares insertion time, split count, tree height, node count, entry count, and utilization.

```bash
./project1 experiment
# Select experiment: 1
```

#### Experiment 2: Point Search Performance

Builds each tree once per order, generates 10,000 random point-search queries, and reports search performance.

```bash
./project1 experiment
# Select experiment: 2
```

#### Experiment 3: Range Query Performance

Runs range queries over two Student ID intervals and calculates aggregate values for matching records:

- `202000000` to `202100000`
- `202000000` to `202600000`

```bash
./project1 experiment
# Select experiment: 3
```

#### Experiment 4: Deletion Performance

Deletes random sets of keys from each tree and records the structural state before and after deletion.

Workloads:

- Delete 10% of records
- Delete 20% of records

```bash
./project1 experiment
# Select experiment: 4
```

#### Experiment 5: Intra-node Search Optimization

Benchmarks the optimized B\*-Tree implementation with different node-internal search strategies:

- Linear search
- Binary search
- Binary search with a 64-entry hot-key cache

The experiment builds each tree from the full dataset and executes 100,000 point-search queries per measured run. It compares the strategies over three query distributions:

- Uniform random lookups
- Hotspot workload, where 80% of queries target 1% of keys
- Zipfian workload with theta `0.99`

Tested tree orders:

```text
16, 32, 64, 128, 256, 512, 1024
```

The order `1024` run is generated by the experiment code, but it is not included in the report.

```bash
./project1 experiment
# Select experiment: 5
```

#### Experiment 6: Selective Redistribution in B\*-tree

Compares insertion behavior for a baseline B-Tree and two optimized B\*-Tree redistribution policies:

- `btree`
- `bstar_eager`
- `bstar_selective_alpha_0_25`

The selective redistribution policy skips some sibling redistributions when the expected benefit is below alpha `0.25`, then measures the tradeoff between insertion time, split count, redistribution count, moved entries, tree height, node count, and node utilization.

Tested tree orders:

```text
10, 20, 50, 100
```

```bash
./project1 experiment
# Select experiment: 6
```

## Common Parameters and Metrics

- `order`: maximum child capacity parameter used by the tree
- `RID`: zero-based row index of a record in the loaded dataset
- `node_read_count`: logical node reads counted during tree operations
- `sequential_leaf_read_count`: sequential leaf reads counted during B+tree-style range scans
- `simulated_ssd_cost_ms`: simulated storage-access cost
- `total_time_with_ssd_ms`: measured execution time plus simulated SSD cost
- `node_utilization`: percentage of occupied entries relative to node capacity
- `rsd`: relative standard deviation used to stop repeated benchmark runs once measurements stabilize
- `intra_node_search_count`: number of searches performed inside tree nodes
- `intra_node_key_comparisons`: key comparisons made during node-internal search
- `hot_key_cache_hits` / `hot_key_cache_misses`: cache behavior for repeated point-search keys in Experiment 5
- `redistribution_count`: number of B\*-Tree sibling redistributions during insertion
- `forced_redistribution_count`: redistributions required before splitting can proceed
- `skipped_redistribution_count`: redistribution attempts skipped by the selective policy in Experiment 6
- `two_to_three_split_count`: number of B\*-Tree 2-to-3 split operations
- `redistribution_moved_entries`: entries moved across siblings during redistribution

## Project Structure

```text
.
|-- Makefile
|-- README.md
|-- data/
|   `-- student.csv
|-- results_experiment/
|   `-- experiment result CSV files
`-- src/
    |-- dataset_handler/
    |-- experiment/
    |-- index_tree/
    `-- main.cpp
```

---

This project was developed for CSE321 Database Systems Project 1 and focuses on index tree implementation, query processing behavior, and experimental comparison of tree variants.
