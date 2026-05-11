# An extended version of CCBench

[CCBench](https://github.com/thawk105/ccbench)[1] is a benchmark platform for various concurrency control protocols. This repository provides an extended version of CCBench, which supports additional workloads and protocols.

## Workloads

An extended CCBench provides the following workloads.

- [BoMB](#bomb)
- [TPC-C](#tpc-c)
- [YCSB](#ycsb)

### BoMB

#### Overview

Product costing using BoM, a hierarchical list of items and their costs to create a product, is a major task in manufacturing databases. Traditionally, this is done when OLTP systems are offline, or online by using stale materialized views. However, there's now a need for on-demand costing due to frequent changes in raw material costs caused by supply chain disruptions. Accurate, real-time cost calculation is crucial for optimizing supply chains.

BoMB (BoM Benchmark) emulates such on-demand costing based on a real bread manufacturing company, [Andersen](https://www.andersen-group.jp/english/), extracting common components and parameters applicable to various industries. The benchmark consists of one long transaction (L1) for product costing using BoM and five short transactions (S1-S5) on the seven tables.

#### Tables

BoMB uses the following seven tables. The underlined attribute is the primary key. `INT16`, `INT32`, and `INT64` are integers of 16, 32, and 64 bits, respectively. The cardinality of the tables are determined by the adjustable [parameters](#parameters).

| Name                | Attributes                                                                                                                                                              | Description                                                                                                                                                                                                                                                                                                                                                                                                                     | Default Cardinality  |
|:--------------------|:------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:---------------------|
| **factory**         | <ul><li><u>id</u> `INT32`</li> <li>name `VARCHAR`</li></ul>                                                                                                             | This table manages a list of factories in a company. The cardinality is given by the `factories` parameter.                                                                                                                                                                                                                                                                                                                     | 8                    |
| **item**            | <ul><li><u>id</u> `INT32`</li> <li>name `VARCHAR`, type `INT16`</li></ul>                                                                                               | This table manages the name of items with their type: product, material, or raw material. The cardinality is given by the sum of the three parameters: `product-types`, `material-types`, and `raw-material-types`.                                                                                                                                                                                                             | 345,000              |
| **product**         | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>quantity `DOUBLE`</li></ul>                                                                  | This table manages the manufactured products and their quantity in each factory. In product costing, it is used to obtain the products currently in production at the factory. The cardinality is given by the product of the parameters `factories` and `target-products`.                                                                                                                                                     | 800                  |
| **bom**             | <ul><li><u>parent_item_id</u> `INT32`</li> <li><u>child_item_id</u> `INT32`</li> <li>quantity `DOUBLE`</li></ul>                                                        | This table manages a list of (intermediate and raw) materials and the quantities of each needed to manufacture a product. Records in this table have a hierarchical structure and logically represents BoM trees. The average cardinality is given by the product of the parameters `product-types`, `product-types`, `material-trees-per-product`, `material-tree-size`, `material-tree-size`/2, and `raw-materials-per-leaf`. | 54,000,000 (average) |
| **material-cost**   | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>stock_quantity `DOUBLE`</li> <li>stock_amount `DOUBLE`</li></ul>                             | This table manages the cost of raw materials for each factory and item. The cardinality is given by the product of the parameters `factories` and `raw-material-types`.                                                                                                                                                                                                                                                         | 600,000              |
| **result-cost**     | <ul><li><u>factory_id</u> `INT32`</li> <li><u>item_id</u> `INT32`</li> <li>result_cost `DOUBLE`</li></ul>                                                               | This table contains the latest cost calculation results for each product in each factory. The cardinality is given by the product of the parameters `factories` and `target-products`.                                                                                                                                                                                                                                          | 800                  |
| **journal-voucher** | <ul><li><u>voucher_id</u> `INT64`</li> <li>date `DATE`</li> <li>debit `INT32`</li> <li>credit `INT32`</li> <li>amount `DOUBLE`</li> <li>description `VARCHAR`</li></ul> | A jounal voucher is inserted into this table when an event happens with the current product cost in the result-cost table. The table is empty when the benchmark started.                                                                                                                                                                                                                                                       | 0                    |

#### Transactions

- **L1 (update-product-cost):** The L1 transaction is a long transaction that builds BoM trees and calculates product costs. First, it selects one factory randomly and obtains all products manufactured at the factory and their quantity by referring to the `product` table. Next, it builds a BoM tree for the product and calculates the cost. When building the BoM tree, it refers to the `material-cost` table, updated by S1 transactions, in addition to the `bom` table. Then, it writes the result to the `result-cost` table, which is referred to by S2 transactions. These steps are repeated for all products.
- **S1 (update-material-cost):** The S1 transaction is a short transaction that changes the cost of raw materials. It selects a factory and a raw material randomly, then read-modify-writes the selected record of the `material-cost` table.
- **S2 (issue-journal-voucher):** The S2 transaction is a short transaction that creates a journal voucher based on the calculated product cost. It selects a factory randomly and scans the `result-cost` table to obtain the cost of each product in the factory. Then, it calculates the amount from the cost and volume (given as inputs) for each product. Finally, it inserts the voucher records into the `journal-voucher` table with the account titles. The number of inserted records depends on the parameter `target-products`.
- **S3 (change-product):** The S3 transaction is a short transaction that replaces an old product with a newly-developed product. It selects a product from a factory randomly and deletes the product. Then it decides a unique item ID for a new product and chooses root materials randomly according to `material-trees-per-product`. Finally, new records with the chosen item ID are inserted into the `bom` table.
- **S4 (change-raw-material):** The S4 transaction is a short transaction that replaces a raw material of a product with a different one (e.g., change a flour X to X'). It randomly selects a record from the `bom` table and raw material from the `item` table. It may use the cached records in the `bom` and `item` tables. Then it deletes the old record and inserts a new record with the chosen raw material.
- **S5 (change-product quantity):** The S5 transaction is a short transaction that updates a manufacturing quantity of a product in a factory, for example, as a result of demand planning. It randomly selects a factory and a product and then updates the record in the `product` table with a given quantity.

#### Parameters

| Name                         | Description                                   | Default   |
|:-----------------------------|:----------------------------------------------|:----------|
| `factories`                  | Number of factories.                          | `8`       |
| `product-types`              | Number of product types.                      | `72,000`  |
| `material-types`             | Number of material types.                     | `198,000` |
| `raw-material-types`         | Number of raw material types.                 | `75,000`  |
| `material-trees-per-product` | Number of material trees per product.         | `5`       |
| `material-tree-size`         | Number of materials a material tree.          | `10`      |
| `raw-meterials-per-leaf`     | Number of raw materials in a leaf material.   | `3`       |
| `target-products`            | Number of products manufactured in a factory. | `100`     |
| `target-materials`           | Number of raw materials for update.           | `1`       |

### TPC-C

TPC-C is a de facto standard benchmark for OLTP systems that simulates a warehouse-centric order processing application. See [the official documents](https://www.tpc.org/tpcc/) for details of the specification. We support the full mix of the TPC-C five transactions. You can specify the number of warehouses as a scale factor and the percentages of five transactions.

### YCSB

[YCSB](https://github.com/brianfrankcooper/YCSB) is a micro benchmark for evaluating the performance of different "key-value" and "cloud" serving stores, which also used for transactional database systems. We provide the following options:

- Number of operations in a single transaction
- Read ratio of a transaction
- Choose read-modify-write or blind write when writing
- Total number of records in a table
- Zipfian skew when choosing accessing records

## Getting Started

### Build

First, clone the repository and install the required packages. Note that the following steps are for Ubuntu. For other Linux distributions, install the equivalent packages.

```sh
$ git clone --recurse-submodules this_repository
$ cd ccbench
$ sudo apt update -y && sudo apt-get install -y $(cat build_tools/ubuntu.deps)
```

Then, build a dependent library.

```sh
$ cd ccbench
$ ./build_tools/bootstrap.sh
$ ./build_tools/bootstrap_mimalloc.sh
```

Finally, build benchmark binaries with all supported protocols.

```sh
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```

### Run

You can run three benchmark workloads with various protocols: YCSB, TPC-C, and BoMB. BoMB (Bill of Materials Benchmark) is a new OLTP benchmark reproducing a workload for manufacturing companies, including long-running update transactions and various short transactions.

For example, if you want to run BoMB with Silo, run the following command.

```sh
$ cd build
$ ./silo/bomb_silo.exe --thread-num 8 --bomb-mixed-mode --bomb-mixed-short-rate 1000
```

Workloads and protocols can be switched like the following.

```sh
$ ./tictoc/bomb_tictoc.exe --thread-num 8 --bomb-mixed-mode --bomb-mixed-short-rate 1000
$ ./cicada/tpcc_cicada.exe --thread-num 8 --tpcc-num-wh 8
$ ./cicada/ycsb_cicada.exe --thread-num 8 --ycsb-rratio 50
```

See usage with the `--help` option for details of the workload-specific options.

## References

```
[1] Takayuki Tanabe, Takashi Hoshino, Hideyuki Kawashima, and Osamu Tatebe. 2020.
    An analysis of concurrency control protocols for in-memory databases with CCBench.
    Proc. VLDB Endow. 13, 13 (September 2020), 3531–3544.
    https://doi.org/10.14778/3424573.3424575
```
