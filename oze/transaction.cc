
#include <stdio.h>
#include <string.h>  // memcpy
#include <xmmintrin.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "include/atomic_tool.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/txid.hh"
#include "include/version.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);
extern void displaySLogSet();
extern void displayDB();
extern void ozeLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);
extern std::vector<Result> OzeResult;

using namespace std;

#define MSK_TID 0b11111111

/**
 * @brief Initialize function of transaction.
 * Allocate timestamp.
 * @return void
 */
void TxExecutor::begin() {
    this->status_ = TransactionStatus::inflight;

    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    auto e = get_min_epoch(); // align with slowest worker's epoch
    if (this->txid_.epoch != e) {
        this->txid_.tid = 0;
    } else
        this->txid_.tid++;
    this->txid_.epoch = e;
    this->reclamation_epoch_ = e - 1;

    this->cc_mode_ = get_next_cc_mode();
    ThManagementTable[thid_]->atomicStoreMode(this->cc_mode_);
    this->occ_guard_required_ = some_threads_in(OCC_MODE|TO_OCC_MODE);

    // reset a flag for insert-only optimization
    this->has_write_ = false;
    this->has_insert_ = false;

    // just for statistics
    this->is_forwarded_ = false;

#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " begin by thread# " << txid_.thid << std::endl;
#endif
}

/**
 * @brief Transaction read function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::read(Storage s, std::string_view key, TupleBody** body) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

    Status ret = Status::OK;
    TxID target;
    bool isInvisible;
    Tuple *tuple;
    Version *ver = nullptr;
    ReadElement<Tuple>* re;
    WriteElement<Tuple>* we;
#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " read " << key << " start" << std::endl;
#endif

    /**
     * read-own-writes or re-read from local read set.
     */
    re = searchReadSet(s, key);
    if (re) {
        *body = &(re->ver_->body_);
        goto FINISH_READ;
    }
    we = searchWriteSet(s, key);
    if (we) {
        *body = &(we->new_ver_->body_);
        goto FINISH_READ;
    }

    /**
     * Search versions from data structure.
     */
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
    if (tuple == nullptr) {
        ret = Status::WARN_NOT_FOUND;
        goto FINISH_READ;
    }

    if (IS_OCC(cc_mode_)) {
      ret = read_internal_occ(s, key, tuple, &ver);
    } else {
      ret = read_internal(s, key, tuple, &ver);
    }
    if (ret == Status::WARN_NOT_FOUND) {
        goto FINISH_READ;
    }
    if (ret != Status::OK) {
        goto ABORT_READ;
    }

    /**
     * Read payload.
     */
    *body = &(ver->body_);

FINISH_READ:
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " read " << key << " done" << std::endl;
#endif
#if ADD_ANALYSIS
    result_->local_read_latency_ += rdtscp() - start;
#endif
    return ret;

ABORT_READ:
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " read " << key << " aborted" << std::endl;
#endif
#if ADD_ANALYSIS
    result_->local_read_latency_ += rdtscp() - start;
#endif
    this->status_ = TransactionStatus::aborted;
    return Status::WARN_NOT_FOUND;
}

Status TxExecutor::read_internal(Storage s, std::string_view key, Tuple* tuple, Version** return_ver) {
    TxID target;
    bool isInvisible;
    Version *ver = nullptr;
    ReadElement<Tuple>* re;
    WriteElement<Tuple>* we;
    Status stat = Status::OK;

    tuple->lock_.w_lock();

    // sync pages that I read
    if (!reconnoitering_) {
      tuple->graph_.emplace(this->txid_, TxNode(this->txid_));
    }
#ifdef SYNC_GRAPH_AGGRESSIVELY
    for (const auto& [tpl, id] : this->readSet_) {
        tuple->graph_.emplace(id, TxNode(id));
        tuple->graph_.at(id).readBy_.emplace(this->txid_);
        tuple->graph_.at(this->txid_).readSet_.emplace(tpl, id);
        tuple->graph_.at(this->txid_).from_.emplace(id);
    }
#endif

#ifndef NAIVE_VERSION_SELECTION
    stat = get_visible_version(tuple, &ver);
#else
    stat = get_aligned_visible_version(tuple, &ver);
#endif

    if (stat != Status::OK) {
        goto OUT;
    }
    if (ver == nullptr) {
        ERR;
    }
    if (ver->status_ == VersionStatus::deleted) {
        stat = Status::WARN_NOT_FOUND;
        goto OUT;
    }
    *return_ver = ver;
    target = ver->txid_;

    if (!reconnoitering_) {
      tuple->graph_.at(this->txid_).readSet_.emplace(tuple, target);
      tuple->graph_.at(this->txid_).from_.emplace(target);
    }
#if MERGE_ON_READ
    merge(this->graph_, tuple->graph_);
#endif

    // TODO: read/write set management should be refactored
    this->read_set_.emplace_back(s, key, tuple, ver, target, tuple->tuple_id_);
    this->readSet_.emplace(tuple, target);

OUT:
    tuple->lock_.w_unlock();
    return stat;
}

Status TxExecutor::read_internal_occ(Storage s, std::string_view key, Tuple* tuple, Version** return_ver) {
  Status stat;
  TupleId expected, check;

  for (;;) {
    expected.obj_ = loadAcquire(tuple->tuple_id_.obj_);
    stat = get_visible_version_occ(tuple, return_ver);
    check.obj_ = loadAcquire(tuple->tuple_id_.obj_);
    if (stat != Status::OK) {
      return stat;
    }
    if (expected == check) {
      break;
    }
  }

  Version *ver = *return_ver;
  this->read_set_.emplace_back(s, key, tuple, ver, ver->txid_, expected);
  return stat;
}

/**
 * @brief Transaction write function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::write(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " write " << key << " start" << std::endl;
#endif
    Status ret = Status::OK;
    Tuple *tuple;

    /**
     * Update  from local write set.
     * Special treat due to performance.
     */
    if (searchWriteSet(s, key)) goto FINISH_WRITE;

    ReadElement<Tuple> *re;
    re = searchReadSet(s, key);
    if (re) {
        /**
         * If it can find record in read set, use this for high performance.
         */
        tuple = re->rcdptr_;
    } else {
        /**
         * Search record from data structure.
         */
        tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
        ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
        if (tuple == nullptr) {
            ret = Status::WARN_NOT_FOUND;
            goto FINISH_WRITE;
        }
    }

    if (IS_OZE(cc_mode_)) {
      this->writeSet_.emplace(tuple);
    }

    Version *new_ver;
    new_ver = newVersionGeneration(tuple, std::move(body));
    write_set_.emplace_back(s, key, tuple, new_ver, OpType::UPDATE);
    has_write_ = true;

FINISH_WRITE:
#if ADD_ANALYSIS
    result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " write " << key << " done" << std::endl;
#endif
  return ret;
}

/**
 * @brief Transaction insert function.
 * @param [in] key The key of key-value
 */
Status TxExecutor::insert(Storage s, std::string_view key, TupleBody&& body) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " insert " << key << " start" << std::endl;
#endif
    if (searchWriteSet(s, key)) return Status::WARN_ALREADY_EXISTS;

    Tuple* tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif
    if (tuple != nullptr) {
        return Status::WARN_ALREADY_EXISTS;
    }

    tuple = new Tuple();
    tuple->init(this->txid_, std::move(body));

    if (IS_OCC(cc_mode_)) {
      typename MasstreeWrapper<Tuple>::insert_info_t insert_info;
      Status stat = Masstrees[get_storage(s)].insert_value(key, tuple, &insert_info);
      if (stat == Status::WARN_ALREADY_EXISTS) {
        delete tuple;
        return stat;
      }
      if (insert_info.node) {
        if (!node_map_.empty()) {
          auto it = node_map_.find((void*)insert_info.node);
          if (it != node_map_.end()) {
            if (unlikely(it->second != insert_info.old_version)) {
              status_ = TransactionStatus::aborted;
              return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
            }
            // otherwise, bump the version
            it->second = insert_info.new_version;
          }
        }
      } else {
        ERR;
      }
    } else {
      Status stat = Masstrees[get_storage(s)].insert_value(key, tuple);
      if (stat == Status::WARN_ALREADY_EXISTS) {
        delete tuple;
        return stat;
      }
      this->writeSet_.emplace(tuple);
    }

    write_set_.emplace_back(s, key, tuple, tuple->ldAcqLatest(), OpType::INSERT);
    has_insert_ = true;

#if ADD_ANALYSIS
    result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
    std::cout << "TxID " << txid_ << " insert " << key << " done" << std::endl;
#endif
  return Status::OK;
}

Status TxExecutor::delete_record(Storage s, std::string_view key) {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
  std::cout << "TxID " << txid_ << " delete " << key << " start" << std::endl;
#endif

  // cancel previous write
  for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
    if ((*itr).storage_ != s) continue;
    if ((*itr).key_ == key) {
      write_set_.erase(itr);
    }
  }

  Tuple* tuple;
  ReadElement<Tuple> *re;
  re = searchReadSet(s, key);
  if (re) {
    tuple = re->rcdptr_;
  } else {
    tuple = Masstrees[get_storage(s)].get_value(key);
#if ADD_ANALYSIS
    ++result_->local_tree_traversal_;
#endif  // if ADD_ANALYSIS
    if (tuple == nullptr) return Status::WARN_NOT_FOUND;
  }

  if (IS_OZE(cc_mode_)) {
    this->writeSet_.emplace(tuple);
  }

  Version *new_ver;
  new_ver = newVersionGeneration(tuple);
  write_set_.emplace_back(s, key, tuple, new_ver, OpType::DELETE);
  has_write_ = true; // since delete is a sort of update

#if ADD_ANALYSIS
  result_->local_write_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS

#if DEBUG_MSG
  std::cout << "TxID " << txid_ << " delete " << key << " done" << std::endl;
#endif
  return Status::OK;
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody*>& result) {
  return scan(s, left_key, l_exclusive, right_key, r_exclusive, result, -1);
}

Status TxExecutor::scan(const Storage s,
                        std::string_view left_key, bool l_exclusive,
                        std::string_view right_key, bool r_exclusive,
                        std::vector<TupleBody*>& result, int64_t limit) {
  result.clear();
  auto rset_init_size = read_set_.size();

  if (!reconnoitering_ && !IS_OCC(cc_mode_)) {
    addScanEntry(s, left_key, l_exclusive, right_key, r_exclusive);
  }

  std::vector<Tuple*> scan_res;
  if (IS_OCC(cc_mode_)) {
    Masstrees[get_storage(s)].scan(
        left_key.empty() ? nullptr : left_key.data(), left_key.size(),
        l_exclusive, right_key.empty() ? nullptr : right_key.data(),
        right_key.size(), r_exclusive, &scan_res, limit, callback_);
  } else {
    Masstrees[get_storage(s)].scan(
        left_key.empty() ? nullptr : left_key.data(), left_key.size(),
        l_exclusive, right_key.empty() ? nullptr : right_key.data(),
        right_key.size(), r_exclusive, &scan_res, limit);
  }

  for (auto &&itr : scan_res) {
    // TODO: Tuple should have key? Accessing key through the latest ver is ugly
    // Must be a copy to avoid buffer overflow when changing the latest
    std::string key(itr->latest_.load(memory_order_acquire)->body_.get_key());
    ReadElement<Tuple>* re = searchReadSet(s, key);
    if (re) {
      result.emplace_back(&(re->ver_->body_));
      continue;
    }

    WriteElement<Tuple>* we = searchWriteSet(s, std::string_view(key));
    if (we) {
      result.emplace_back(&(we->new_ver_->body_));
      continue;
    }

    Version* ver = nullptr;
    Status stat;
    if (IS_OCC(cc_mode_)) {
      stat = read_internal_occ(s, key, itr, &ver);
    } else {
      stat = read_internal(s, key, itr, &ver);
    }
    if (stat != Status::OK && stat != Status::WARN_NOT_FOUND) {
        status_ = TransactionStatus::aborted;
        return stat;
    }
  }

  if (rset_init_size != read_set_.size()) {
    for (auto itr = read_set_.begin() + rset_init_size;
         itr != read_set_.end(); ++itr) {
      result.emplace_back(&((*itr).ver_->body_));
    }
  }

  return Status::OK;
}

void TxExecutor::validation_worker(size_t worker_id,
    KeySet &target_set, size_t offset, size_t assignment,
    KeySet &propagate_set, Graph &graph, TransactionStatus &result) {
    // copy initial state
    graph.clear();
    graph.insert(this->graph_.begin(), this->graph_.end());
    // std::cout << "[" << this->txid_ << ":" << worker_id << "] "
    //           << "Graph size: " << graph.size() << std::endl;

    // set assignment
    auto iterator = target_set.begin();
    auto end = target_set.begin();
    advance(iterator, offset);
    advance(end, offset + assignment);

    uint64_t num_pages = 0;
    for (; iterator != end; iterator++) {
        if (iterator == target_set.end())
            break;
        Tuple* tuple = *iterator;

        tuple->lock_.w_lock();
        tuple->graph_.emplace(this->txid_, TxNode(this->txid_));
        tuple->graph_.at(this->txid_).status_ = TransactionStatus::validating;

        // std::cout << "[" << this->txid_ << ":" << worker_id << "] "
        //           << "Per-page validation start for " << key << std::endl;

        // merge graph
        merge(tuple->graph_, graph);
        // gc(tuple->graph_, this->reclamation_epoch_);

        if (has_cycle(tuple->graph_, this->txid_)) {
            clean_up_node_edges(tuple->graph_, this->txid_);
            tuple->lock_.w_unlock();
            goto ABORT_VALIDATION;
        }

        tuple->graph_.at(this->txid_).readSet_.insert(
            this->readSet_.begin(), this->readSet_.end()
        );
        tuple->graph_.at(this->txid_).writeSet_.insert(this->writeSet_.begin(), this->writeSet_.end());

        // for propagation
        KeySet keys;
        TxSet reachable_to;
        find_reachable_to(this->txid_, tuple->graph_, reachable_to);
        for (auto& txid : reachable_to) {
            add_related_read_keys(&tuple->graph_.at(txid), keys);
        }
        for (const auto& k : keys) {
            auto itr = std::find(target_set.begin(), target_set.end(), k);
            if (itr == target_set.end())
                propagate_set.insert(k);
        }

        graph.clear();
        graph.insert(tuple->graph_.begin(), tuple->graph_.end());

        // std::cout << "[" << this->txid_ << ":" << worker_id << "] "
        //           << "Per-page validation done for " << key << std::endl;

        tuple->lock_.w_unlock();

        num_pages++;

#if DEBUG_MSG
        if (num_pages % 100 == 0) {
            std::stringstream ss;
            ss << "validation worker"
               << worker_id << " " << num_pages << " done";
        }
#endif

        if (loadAcquire(this->quit_)) {
            result = TransactionStatus::invalid;
            return;
        }

    }
    result = TransactionStatus::validating;
    return;

ABORT_VALIDATION:
    result = TransactionStatus::aborted;
    return;
}

bool TxExecutor::validation() {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS

    int num_pages = 0;
    int num_skip_propagate = 0;

    KeySet finished_set;
    KeySet propagate_set;

    this->graph_.emplace(this->txid_, TxNode(this->txid_));
    this->graph_.at(this->txid_).status_ = TransactionStatus::validating;

    // for read-only transacton (cf. read crown)
    if (this->writeSet_.size() == 0) {
        for (const auto& [tpl, _] : this->readSet_) {
            tpl->lock_.w_lock();
            merge(tpl->graph_, this->graph_);
            if (has_cycle(tpl->graph_, this->txid_)) {
                if (this->status_ != TransactionStatus::invalid)
                    this->status_ = TransactionStatus::aborted;
                tpl->lock_.w_unlock();
                goto ABORT_VALIDATION;
            } else {
                this->graph_.clear();
                this->graph_.insert(tpl->graph_.begin(), tpl->graph_.end());
            }
            tpl->lock_.w_unlock();
        }
        goto FINISH_VALIDATION;
    }

    // insert-only optimization
    if (!has_write_ && has_insert_) {
      for (auto& we : this->write_set_) {
        if (!insert_validation(we))
          goto ABORT_VALIDATION;
        finished_set.insert(we.rcdptr_);
      }
      goto FINISH_VALIDATION;
    }

    for (auto& we : this->write_set_) {
        if (we.op_ == OpType::INSERT) {
            if (!insert_validation(we))
                goto ABORT_VALIDATION;
            finished_set.insert(we.rcdptr_);
        } else { // OpType::UPDATE or OpType::DELETE
            if (!write_validation(we, finished_set, propagate_set))
                goto ABORT_VALIDATION;
        }
        if (loadAcquire(this->quit_)) {
            this->status_ = TransactionStatus::invalid;
            goto FINISH_VALIDATION;
        }
    }

    for (const auto& [tpl, _] : this->readSet_) {
        if (auto it = this->writeSet_.find(tpl); it == this->writeSet_.end())
            propagate_set.insert(tpl);
    }

    if (FLAGS_validation_th_num == 1 ||
        propagate_set.size() <= FLAGS_validation_threshold) {
        bool ret = read_validation(propagate_set, finished_set);
        if (ret && propagate_set.empty()) {
            goto FINISH_VALIDATION;
        } else if (!ret) {
            goto ABORT_VALIDATION;
        }
    }

    while (!propagate_set.empty()) {
        uint64_t workers = 1;
        uint64_t total_task = propagate_set.size();
        uint64_t assignment = total_task;
        if (FLAGS_validation_th_num > 1 &&
            total_task > FLAGS_validation_threshold) {
            workers = FLAGS_validation_th_num;
            assignment = (size_t)ceil((double)total_task/(double)workers);
        }
        std::vector<std::thread> thv;
        std::vector<KeySet> propagate_subset(workers);
        std::vector<Graph> graph(workers);
        TransactionStatus result[workers];

        for (size_t i = 0; i < workers; ++i) {
            propagate_subset[i] = KeySet();
            graph[i] = Graph();
            thv.emplace_back(&TxExecutor::validation_worker, this, i,
                std::ref(propagate_set), assignment*i, assignment,
                std::ref(propagate_subset[i]), std::ref(graph[i]),
                std::ref(result[i]));
        }
        for (auto &th : thv) th.join();

        for (size_t i = 0; i < workers; ++i) {
            if (result[i] == TransactionStatus::validating) {
                // std::cout << "Merging results from a worker: " << i << std::endl;
                merge(this->graph_, graph[i]);
            } else if (result[i] == TransactionStatus::aborted) {
                // std::cout << "Validation failed in a worker: " << i << std::endl;
                goto ABORT_VALIDATION;
            } else if (result[i] == TransactionStatus::invalid) {
                // cancel due to timeout
                this->status_ = TransactionStatus::invalid;
                goto FINISH_VALIDATION;
            } else {
                ERR;
            }
        }

        if (has_cycle(this->graph_, this->txid_)) {
            // TODO: should we clean up edges on each tuple?
            // cf. clean_up_node_edges(tuple->graph_, this->txid_);
            goto ABORT_VALIDATION;
        }

        finished_set.insert(propagate_set.begin(), propagate_set.end());
        propagate_set.clear();

        for (size_t i = 0; i < workers; ++i) {
            for (auto& k : propagate_subset[i]) {
                if (auto it = finished_set.find(k); it == finished_set.end()) {
                    propagate_set.insert(k);
                }
            }
        }

        if (loadAcquire(this->quit_)) {
            this->status_ = TransactionStatus::invalid;
            goto FINISH_VALIDATION;
        }
    }

FINISH_VALIDATION:
#if ADD_ANALYSIS
    result_->local_vali_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return true;

ABORT_VALIDATION:
#if ADD_ANALYSIS
    result_->local_vali_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return false;
}

bool TxExecutor::write_validation(WriteElement<Tuple> element, KeySet& finished_set, KeySet& propagate_set) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS
#if DEBUG_MSG
    std::cout << "  Per-page write validation start for " << tuple << std::endl;
#endif
    Status stat;
    TxSet decided, reachable_to;
    KeySet followers_read_keys;
    Tuple* tuple = element.rcdptr_;
    OpType op = element.op_;

    tuple->lock_.w_lock();

    tuple->graph_.emplace(this->txid_, TxNode(this->txid_));
    tuple->graph_.at(this->txid_).status_ = TransactionStatus::validating;

    // merge graph
    merge(tuple->graph_, this->graph_);
    gc(tuple->graph_, this->reclamation_epoch_);

#if DEBUG_MSG
    std::cout << "  Merged done, now find readers" << std::endl;
#endif
    // add edges and check my RF
    // 1. find readers (who reads write-target pages?)
    // 2. add  this txid to readers' writtenBy list
    // 3. check my RF
    TxSet readers;
    TxSet followers;
    find_readers(tuple->graph_, tuple, readers);
    for (const auto& r : readers) {
#if DEBUG_MSG
        std::cout << "    Reader: " << r << std::endl;
#endif
        if (this->txid_.epoch < r.epoch - 1) {
            goto ABORT_VALIDATION;
        }
        tuple->graph_.at(r).writtenBy_.insert(this->txid_);
        tuple->graph_.at(this->txid_).from_.insert(r);
    }

    if (has_cycle(tuple->graph_, this->txid_)) {
#if DEBUG_MSG
        std::cout << "  Cycle: " << this->txid_ << " Tuple: " << tuple << std::endl;
        std::cout << "  Try order forwarding: " << tuple << std::endl;
#endif
        if (op == OpType::DELETE || !FLAGS_forwarding) {
            goto ABORT_VALIDATION;
        }
        find_followers(tuple->graph_, tuple, readers, followers);
        for (const auto& r : readers) {
            if (auto it = decided.find(r); it == decided.end()) {
                tuple->graph_.at(r).writtenBy_.erase(this->txid_);
                tuple->graph_.at(this->txid_).from_.erase(r);
            }
        }
        for (const auto& f : followers) {
            if (f.epoch < this->txid_.epoch - 1) {
                // Do not forward to the position before the tx in the previous epoch
                goto ABORT_VALIDATION;
            }
            tuple->graph_.at(this->txid_).writtenBy_.insert(f);
            tuple->graph_.at(f).from_.insert(this->txid_);
        }
        if (has_cycle(tuple->graph_, this->txid_)) {
            goto ABORT_VALIDATION;
        }
    } else {
        for (const auto& r : readers) {
            decided.insert(r);
        }
    }

    tuple->graph_.at(this->txid_).readSet_.insert(
        this->readSet_.begin(), this->readSet_.end()
    );
    tuple->graph_.at(this->txid_).writeSet_.insert(this->writeSet_.begin(), this->writeSet_.end());

    // insert write version to the appropriate position in the linked list
    stat = insert_version(element, followers);
    if (stat != Status::OK) {
        goto ABORT_VALIDATION;
    }

#if DEBUG_MSG
    std::cout << "  Per-page write validation done for " << tuple << std::endl;
    print_record(tuple);
    print_transaction();
    // generate_dot_graph("validation"+std::to_string(key), tuple->graph_);
    std::cout << "  Find propagation targets" << std::endl;
#endif

    // for propagation
    find_reachable_to(this->txid_, tuple->graph_, reachable_to);
    for (auto& txid : reachable_to) {
        add_related_read_keys(&tuple->graph_.at(txid), followers_read_keys);
    }
    for (const auto& k : followers_read_keys) {
        push_candidate_keys(propagate_set, finished_set, followers_read_keys);
    }

    this->graph_.clear();
    this->graph_.insert(tuple->graph_.begin(), tuple->graph_.end());

    tuple->lock_.w_unlock();
    finished_set.insert(tuple);
    if (loadAcquire(this->quit_)) {
        this->status_ = TransactionStatus::invalid;
        return true;
    }

FINISH_VALIDATION:
#if ADD_ANALYSIS
    result_->local_write_validation_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return true;

ABORT_VALIDATION:
    clean_up_node_edges(tuple->graph_, this->txid_);
    tuple->lock_.w_unlock();
#if ADD_ANALYSIS
    result_->local_write_validation_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return false;
}

bool TxExecutor::insert_validation(WriteElement<Tuple> element) {
    Tuple* tuple = element.rcdptr_;
    Storage s = element.storage_;
    string_view key = element.key_;
    ScanEntry *prev, *entry;
    int history_offset = SCAN_HISTORY_INDEX(s, this->txid_.epoch);

#if DEBUG_MSG
    std::cout << "  Per-page insert validation start for " << tuple << std::endl;
#endif
    tuple->lock_.w_lock();

    // check scan index
    uint64_t range = ScanRange[history_offset].load(std::memory_order_acquire);
    uint32_t min = range >> 32;
    uint32_t max = range & 0xffffffff;
    uint32_t prefix = *reinterpret_cast<const uint32_t*>(key.data());
    if (prefix < min || max < prefix) {
      goto OUT;
    }

    // check scan history
    prev = nullptr;
    entry = ScanHistory[history_offset].load(std::memory_order_acquire);
    while (entry && entry->txid_.epoch == this->txid_.epoch) {
#if DEBUG_MSG
        std::cout << "  Scan entry: " << tuple << std::endl;
#endif
        if (entry->txid_ != txid_ && entry->contains(element.key_)) {
            // add edge; scanner to inserter
            if (auto it = this->graph_.find(entry->txid_); it == this->graph_.end())
                this->graph_.emplace(entry->txid_, TxNode(entry->txid_));
            this->graph_.at(entry->txid_).writtenBy_.insert(this->txid_);
            this->graph_.at(this->txid_).from_.insert(entry->txid_);
            // check cycle
            if (has_cycle(this->graph_, this->txid_)) {
                tuple->lock_.w_unlock();
                return false;
            }
        }
        prev = entry;
        entry = entry->ldAcqNext();
    }

    if (entry && entry->txid_.epoch <= reclamation_epoch_) {
        if (prev == nullptr) {
            if (ScanHistory[history_offset].compare_exchange_strong(entry, nullptr,
                memory_order_acq_rel, memory_order_acquire)) {
                delete entry;
            }
        } else {
            if (prev->next_.compare_exchange_strong(entry, nullptr,
                memory_order_acq_rel, memory_order_acquire)) {
                while (entry) {
                    prev = entry;
                    entry = entry->ldAcqNext();
                    delete prev;
                }
            }
        }
    }

OUT:
    // sync graph to log my reads-from at least
    tuple->graph_.insert(this->graph_.begin(), this->graph_.end());
#if DEBUG_MSG
    print_record(tuple);
#endif

    tuple->lock_.w_unlock();
#if DEBUG_MSG
    std::cout << "  Per-page insert validation done for " << tuple << std::endl;
#endif

    return true;
}

bool TxExecutor::read_validation(KeySet& target_set, KeySet& finished_set) {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " read page validation start" << std::endl;
#endif

    while (!target_set.empty()) {
        ReadElement<Tuple> *re;
        WriteElement<Tuple> *we;
        auto iterator = target_set.cbegin();
        Tuple* tuple = *iterator;
        std::string key(tuple->latest_.load(memory_order_acquire)->body_.get_key());

        tuple->lock_.w_lock();

#if DEBUG_MSG
        std::cout << "  Per-page validation start for " << key << std::endl;
#endif
        tuple->graph_.emplace(this->txid_, TxNode(this->txid_));
        tuple->graph_.at(this->txid_).status_ = TransactionStatus::validating;

        // merge graph
        merge(tuple->graph_, this->graph_);
        gc(tuple->graph_, this->reclamation_epoch_);

#if DEBUG_MSG
        std::cout << "  Merged done, check cycle" << std::endl;
#endif

        if (has_cycle(tuple->graph_, this->txid_)) {
#if DEBUG_MSG
            std::cout << "  Cycle: " << this->txid_ << " Key: " << key << std::endl;
#endif
            clean_up_node_edges(tuple->graph_, this->txid_);
            tuple->lock_.w_unlock();
            goto ABORT_VALIDATION;
        }

        tuple->graph_.at(this->txid_).readSet_.insert(
            this->readSet_.begin(), this->readSet_.end()
        );
        tuple->graph_.at(this->txid_).writeSet_.insert(this->writeSet_.begin(), this->writeSet_.end());

#if DEBUG_MSG
        std::cout << "  Per-page validation done for " << key << std::endl;
        print_record(tuple);
        print_transaction();
        // generate_dot_graph("validation"+std::to_string(key), tuple->graph_);
        std::cout << "  Find propagation targets" << std::endl;
#endif

        // for propagation
        KeySet keys;
        TxSet reachable_to;
        find_reachable_to(this->txid_, tuple->graph_, reachable_to);
        for (auto& txid : reachable_to) {
            add_related_read_keys(&tuple->graph_.at(txid), keys);
        }
        for (const auto& k : keys) {
            push_candidate_keys(target_set, finished_set, keys);
        }

        this->graph_.clear();
        this->graph_.insert(tuple->graph_.begin(), tuple->graph_.end());

        tuple->lock_.w_unlock();
        finished_set.insert(tuple);
        target_set.erase(iterator);

        if (FLAGS_validation_th_num != 1 &&
            target_set.size() > FLAGS_validation_threshold) {
            // oops, many propagate found, so fall back parallel validation
#if DEBUG_MSG
            dump(thid_, "fall back parallel validation");
#endif
            return true;
        }

        if (loadAcquire(this->quit_)) {
            this->status_ = TransactionStatus::invalid;
            return true;
        }
    }

FINISH_VALIDATION:
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " read page validation done" << std::endl;
#endif
    // generate_dot_graph("test" + std::to_string(this->txid_.thid), this->graph_);
#if ADD_ANALYSIS
    result_->local_read_validation_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return true;

ABORT_VALIDATION:
#if DEBUG_MSG
    std::cout << "TxID: " << this->txid_ << " aborted" << std::endl;
#endif
#if ADD_ANALYSIS
    result_->local_read_validation_latency_ += rdtscp() - start;
#endif  // if ADD_ANALYSIS
    return false;
}

/**
 * @brief function about abort.
 * clean-up local read/write set.
 * @return void
 */
void TxExecutor::abort() {
#if ADD_ANALYSIS
  uint64_t start = rdtscp();
#endif  // if ADD_ANALYSIS
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " abort process begin" << std::endl;
#endif

    writeSetClean();
    read_set_.clear();
    node_map_.clear();

    graph_.clear();
    readSet_.clear();
    writeSet_.clear();

#ifdef GC_ABORTED_TX
    AbortTx* tx = new AbortTx(this->txid_);
    AbortTx* expected = AbortTxList.load(std::memory_order_acquire);
    tx->strRelNext(expected);
    while (!AbortTxList.compare_exchange_strong(expected, tx,
                memory_order_acq_rel, memory_order_acquire)) {}
#endif

    mainte();

#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " abort process done" << std::endl;
#endif
#if BACK_OFF
    backoff();
#endif
#if ADD_ANALYSIS
  result_->local_abort_latency_ += rdtscp() - start;
#endif
}

bool TxExecutor::validation_occ() {
  TupleId expected, desired;

  sort(write_set_.begin(), write_set_.end());
  lock_write_set();
  if (this->status_ == TransactionStatus::aborted) {
    return false;
  }

  asm volatile("":: : "memory");
  atomicStoreThLocalEpoch(thid_, atomicLoadGE());
  asm volatile("":: : "memory");

  for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr) {
    TupleId check;
    check.obj_ = __atomic_load_n(&((*itr).rcdptr_->tuple_id_.obj_), __ATOMIC_ACQUIRE);
    if ((*itr).tuple_id_.epoch != check.epoch || (*itr).tuple_id_.tid != check.tid) {
      this->status_ = TransactionStatus::aborted;
      unlock_write_set();
      return false;
    }

    if ((*itr).rcdptr_->lock_.counter.load(std::memory_order_acquire) == -1
        && searchWriteSet((*itr).storage_, (*itr).key_) == nullptr) {
      this->status_ = TransactionStatus::aborted;
      unlock_write_set();
      return false;
    }

    this->max_tid_rset_ = max(this->max_tid_rset_, (*itr).rcdptr_->tuple_id_);
  }

  // validate the node set
  for (auto it : node_map_) {
    auto node = (MasstreeWrapper<Tuple>::node_type *) it.first;
    if (node->full_version_value() != it.second) {
      this->status_ = TransactionStatus::aborted;
      unlock_write_set();
      return false;
    }
  }

  return true;
}

void TxExecutor::gc_records() {
  // for records
  while (!gc_records_.empty()) {
    Tuple* rec = gc_records_.front();
    Version* latest = rec->ldAcqLatest();
    if (latest->txid_.epoch > reclamation_epoch_) break;
    if (latest->status_ != VersionStatus::deleted) ERR;
    delete rec;
    gc_records_.pop_front();
  }
}

void TxExecutor::mainte() {
#if ADD_ANALYSIS
  uint64_t start;
  start = rdtscp();
#endif

  gc_records();

#if ADD_ANALYSIS
  result_->local_gc_latency_ += rdtscp() - start;
#endif
}

void TxExecutor::writePhase() {
#if ADD_ANALYSIS
    uint64_t start = rdtscp();
#endif
    if (IS_OCC(cc_mode_) || occ_guard_required_) {
      most_recent_tid_ = decide_tuple_id();
      for (auto we : write_set_) {
        if (we.op_ != OpType::INSERT) {
          insert_version(we);
        }
        commit_version(we);
        storeRelease(we.rcdptr_->tuple_id_.obj_, most_recent_tid_.obj_);
        if (we.op_ != OpType::INSERT) {
          we.rcdptr_->lock_.w_unlock();
        }
      }
    } else {
      for (auto we : write_set_) {
        commit_version(we);
      }
    }

    read_set_.clear();
    write_set_.clear();
    node_map_.clear();
    readSet_.clear();
    writeSet_.clear();
    graph_.clear();
    following_.clear();
    followers_.clear();
#if DEBUG_MSG
    std::cout << "TxID " << this->txid_ << " done" << std::endl;
#endif
#if ADD_ANALYSIS
    result_->local_commit_latency_ += rdtscp() - start;
#endif
}

bool TxExecutor::commit() {
  if (IS_OCC(cc_mode_)) {
    if (cc_mode_ & TO_OZE_MODE) {
      ERR;
    }
    if (!validation_occ()) {
      return false;
    }
    writePhase();
    mainte();
    return true;
  }

  if (!validation()) {
    return false;
  }

  // Abandon ongoing long transaction when time up
  if (status_ == TransactionStatus::invalid)
    return false;

  occ_guard_required_ |= some_threads_in(OCC_MODE|TO_OCC_MODE);
  if (occ_guard_required_ && !validation_occ()) {
    return false;
  }
  writePhase();
  mainte();
  return true;
}

void TxExecutor::reconnoiter_begin() {
    if (status_ != TransactionStatus::inflight) {
      begin();
    }
    reconnoitering_ = true;
}

void TxExecutor::reconnoiter_end() {
    node_map_.clear();
    read_set_.clear();
    readSet_.clear();
    graph_.clear();
    if (write_set_.size() != 0 || writeSet_.size() != 0) {
        dump(thid_, "WARN: Write happened in reconnoitering mode.");
    }
    reconnoitering_ = false;
    begin();
}

bool TxExecutor::isLeader() {
    return this->thid_ == 1;
}

void TxExecutor::leaderWork() {
    ozeLeaderWork(epoch_timer_start_, epoch_timer_stop_);
#if BACK_OFF
    leaderBackoffWork(backoff_, OzeResult);
#endif
}

void TxScanCallback::on_resp_node(const MasstreeWrapper<Tuple>::node_type *n, uint64_t version) {
  auto it = tx_->node_map_.find((void*)n);
  if (it == tx_->node_map_.end()) {
    tx_->node_map_.emplace_hint(it, (void*)n, version);
  } else if ((*it).second != version) {
    tx_->status_ = TransactionStatus::aborted;
  }
}
