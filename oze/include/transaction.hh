#pragma once

#include <stdio.h>
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <stack>
#include <queue>

#include "../../include/backoff.hh"
#include "../../include/config.hh"
#include "../../include/debug.hh"
#include "../../include/inline.hh"
#include "../../include/procedure.hh"
#include "../../include/result.hh"
#include "../../include/status.hh"
#include "../../include/string.hh"
#include "../../include/util.hh"
#include "oze.hh"
#include "oze_op_element.hh"
#include "common.hh"
#include "scan.hh"
#include "scan_callback.hh"
#include "time_stamp.hh"
#include "tuple.hh"
#include "txid.hh"
#include "version.hh"

#define CONTINUING_COMMIT_THRESHOLD 5

class TxScanCallback;

class TxExecutor {
public:
    TransactionStatus status_ = TransactionStatus::invalid;
    std::vector<ReadElement<Tuple>> read_set_;
    std::vector<WriteElement<Tuple>> write_set_;
    std::vector<Procedure> pro_set_;
    std::deque<Tuple*> gc_records_;
    Result *result_ = nullptr;
    Backoff& backoff_;
    uint64_t epoch_timer_start_, epoch_timer_stop_;

    uint8_t cc_mode_;
    ReadSet readSet_;
    WriteSet writeSet_;
    Graph graph_;
    TxSet following_;
    TxSet followers_;
    uint64_t reclamation_epoch_;

    // for OCC
    TupleId max_tid_rset_, max_tid_wset_, most_recent_tid_;
    TxScanCallback callback_;
    std::unordered_map<void*, uint64_t> node_map_;

    TxID txid_;
    bool reconnoitering_ = false;
    bool is_ronly_ = false;
    bool is_batch_ = false;
    bool is_forwarded_;
    bool has_write_ = false;
    bool has_insert_ = false;
    bool occ_guard_required_ = false;
    uint8_t thid_ = 0;

    // for parallel validation
    RWLock lock_;

    const bool& quit_; // for thread termination control

    TxExecutor(uint8_t thid, Backoff& backoff, Result *res, const bool &quit)
        : result_(res), thid_(thid), backoff_(backoff), quit_(quit), callback_(TxScanCallback(this)) {
        txid_.thid = thid_;
        txid_.tid = 0;

        is_batch_ = false;
        is_forwarded_ = false;
    }

    ~TxExecutor() {
        read_set_.clear();
        write_set_.clear();
        pro_set_.clear();
    }

    void abort();

    void displayWriteSet();

    void mainte();  // maintenance

    void begin();

    Status read(Storage s, std::string_view key, TupleBody** body);
    Status read_internal(Storage s, std::string_view key, Tuple* tuple, Version** return_ver);
    Status read_internal_occ(Storage s, std::string_view key, Tuple* tuple, Version** return_ver);

    Status write(Storage s, std::string_view key, TupleBody&& body);

    Status insert(Storage s, std::string_view key, TupleBody&& body);

    Status delete_record(Storage s, std::string_view key);

    Status scan(Storage s,
                std::string_view left_key, bool l_exclusive,
                std::string_view right_key, bool r_exclusive,
                std::vector<TupleBody *>&result);

    Status scan(Storage s,
                std::string_view left_key, bool l_exclusive,
                std::string_view right_key, bool r_exclusive,
                std::vector<TupleBody *>&result, int64_t limit);

    bool read_validation(KeySet &target_set, KeySet &finished_set);
    bool write_validation(WriteElement<Tuple> element, KeySet &finished_set, KeySet &propagate_set);
    bool insert_validation(WriteElement<Tuple> element);
    bool validation();
    bool validation_occ();
    void validation_worker(size_t worker_id, KeySet &propagate_set,
                           size_t offset, size_t assignment,
                           KeySet &propagate_subset, Graph &graph, TransactionStatus &result);

    void writePhase();

    bool commit();

    void reconnoiter_begin();
    void reconnoiter_end();

    bool isLeader();

    void leaderWork();

    void gc_records();

    void backoff() {
#if ADD_ANALYSIS
        uint64_t start = rdtscp();
#endif

        Backoff::backoff(FLAGS_clocks_per_us);

#if ADD_ANALYSIS
        result_->local_backoff_latency_ += rdtscp() - start;
#endif
    }

    Version *newVersionGeneration([[maybe_unused]] Tuple *tuple) {
#if ADD_ANALYSIS
        ++result_->local_version_malloc_;
#endif
        return new Version(this->txid_);
    }

    Version *newVersionGeneration([[maybe_unused]] Tuple *tuple, TupleBody&& body) {
#if ADD_ANALYSIS
        ++result_->local_version_malloc_;
#endif
        return new Version(this->txid_, std::move(body));
    }

    /**
     * @brief Search xxx set
     * @detail Search element of local set corresponding to given key.
     * In this prototype system, the value to be updated for each worker thread 
     * is fixed for high performance, so it is only necessary to check the key match.
     * @param Key [in] the key of key-value
     * @return Corresponding element of local set
     */
    inline ReadElement<Tuple> *searchReadSet(Storage s, std::string_view key) {
        for (auto &re : read_set_) {
            if (re.storage_ != s) continue;
            if (re.key_ == key) return &re;
        }
        return nullptr;
    }

    /**
     * @brief Search xxx set
     * @detail Search element of local set corresponding to given key.
     * In this prototype system, the value to be updated for each worker thread 
     * is fixed for high performance, so it is only necessary to check the key match.
     * @param Key [in] the key of key-value
     * @return Corresponding element of local set
     */
    inline WriteElement<Tuple> *searchWriteSet(Storage s, std::string_view key) {
        for (auto &we : write_set_) {
            if (we.storage_ != s) continue;
            if (we.key_ == key) return &we;
        }

        return nullptr;
    }

    inline WriteElement<Tuple> *searchWriteSet(Tuple* tuple) {
        for (auto &we : write_set_) {
            if (we.rcdptr_ == tuple) return &we;
        }

        return nullptr;
    }

    void writeSetClean() {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
            (*itr).new_ver_->status_.store(VersionStatus::aborted, std::memory_order_release);
            if ((*itr).op_ == OpType::INSERT) {
                Masstrees[get_storage((*itr).storage_)].remove_value((*itr).key_);
                delete (*itr).new_ver_;
                delete (*itr).rcdptr_;
            }
        }
        write_set_.clear();
    }

    void addScanEntry(const Storage s,
                      std::string_view left_key, bool l_exclusive,
                      std::string_view right_key, bool r_exclusive) {
        auto index = SCAN_HISTORY_INDEX(s, this->txid_.epoch);
        while(true) {
            uint64_t expected = ScanRange[index].load(std::memory_order_acquire);
            uint32_t min = expected >> 32;
            uint32_t max = expected & 0xffffffff;
            uint32_t left = *reinterpret_cast<const uint32_t*>(left_key.data());
            uint32_t right = *reinterpret_cast<const uint32_t*>(right_key.data());
            uint64_t updated;
            if (max < right) {
              updated |= static_cast<uint64_t>(right) & 0xffffffff;
            } else {
              updated |= static_cast<uint64_t>(max) & 0xffffffff;
            }
            if (left < min) {
              updated |= static_cast<uint64_t>(left) << 32;
            } else {
              updated |= static_cast<uint64_t>(min) << 32;
            }
            if (expected != updated) {
              if (ScanRange[index].compare_exchange_strong(
                  expected, updated, memory_order_acq_rel, memory_order_acquire))
                break;
            } else {
              break;
            }
        }
        while(true) {
          ScanEntry* expected = ScanHistory[index].load(std::memory_order_acquire);
          ScanEntry* new_entry = new ScanEntry(txid_, left_key, l_exclusive,
                                               right_key, r_exclusive, expected);
          if (ScanHistory[index].compare_exchange_strong(expected, new_entry,
                                                         memory_order_acq_rel, memory_order_acquire))
            break;
        }
    }

    static INLINE Tuple *get_tuple(Tuple *table, uint64_t key) {
        return &table[key];
    }

    /**
     * Oze specific helper functions
     */
    bool has_rw_conflict(const ReadSet &rset, const WriteSet &wset) {
        for (const auto& [key, _] : rset) {
            if (auto it = wset.find(key); it != wset.end())
                return true;
        }
        return false;
    }

    bool has_any_conflict(const ReadSet &my_read_set, const WriteSet &my_write_set,
                      const ReadSet &your_read_set, const WriteSet &your_write_set) {
        for (auto& [key, _] : my_read_set) {
            auto it = your_write_set.find(key);
            if (it != your_write_set.end()) {
                return true;
            }
        }
        for (auto& key : my_write_set) {
            auto rit = your_read_set.find(key);
            if (rit != your_read_set.end()) {
                return true;
            }
            auto wit = your_write_set.find(key);
            if (wit != your_write_set.end()) {
                return true;
            }
        }
        return false;
    }

    void find_readers(const Graph &g, const Key key, TxSet &readers) {
        for (const auto& [txid, node] : g) {
            if (txid == this->txid_) continue; // to avoid including me in the RMW case
            if (g.at(txid).is_aborted) continue;
            if (auto it = g.at(txid).readSet_.find(key);
                it !=g.at(txid).readSet_.end()) {
                readers.insert(txid);
            }
        }
    }
    
    void find_followers(const Graph &g, const Key key, TxSet &readers, TxSet &followers) {
        for (const auto& reader : readers) {
            followers.insert(g.at(reader).readSet_.at(key));
        }
    }

    void find_reachable_to_dfs(const Graph &g, std::map<TxID,bool> &seen, const TxID txID) {
        seen.emplace(txID, true);
        TxSet edges(g.at(txID).readBy_);
        edges.insert(g.at(txID).writtenBy_.begin(), g.at(txID).writtenBy_.end());

        for (const auto& next : edges) {
            if (auto it = seen.find(next); it != seen.end() && seen[next]) continue;
            if (auto it = g.find(next); it != g.end())
                find_reachable_to_dfs(g, seen, next);
        }
    }

    void find_reachable_to(const TxID start, const Graph &g, TxSet &txns) {
        // find transactions that can be visited from start transaction
        std::map<TxID,bool> seen;
        for (const auto& next : g.at(start).readBy_) {
            // TODO: This kind of checking might be skipped if merge/GC works well-mannered
            if (auto it = g.find(next); it != g.end())
                find_reachable_to_dfs(g, seen, next);
        }
        for (const auto& next : g.at(start).writtenBy_) {
            if (auto it = g.find(next); it != g.end())
                find_reachable_to_dfs(g, seen, next);
        }
        for (const auto& [id, _] : seen)
            txns.insert(id);
    }

    void find_reachable_from_dfs(const Graph &g, std::map<TxID,bool> &seen, const TxID txID) {
        seen.emplace(txID, true);
        TxSet edges(g.at(txID).from_);

        for (const auto& from : edges) {
            if (auto it = seen.find(from); it != seen.end() && seen[from]) continue;
            if (auto it = g.find(from); it == g.end()) // TODO: why is this check necessary?
                continue;
            find_reachable_from_dfs(g, seen, from);
        }
    }

    void find_reachable_from(const TxID target, const Graph &g, TxSet &txns) {
        // find transactions that can visit the target transaction
        std::map<TxID,bool> seen;
        find_reachable_from_dfs(g, seen, target);
        for (const auto& [id, _] : seen)
            txns.insert(id);
    }

    bool is_invisible_dfs(Key key, TxID version, TxID myTxID, Graph &g, TxID txID, bool invisible,
                          std::map<TxID,bool> &seen, std::map<TxID,bool> &finished, std::vector<TxID> &stack) {
#if DEBUG_MSG
        std::cout << "    Checking TxID " << txID << " currently : " << invisible << std::endl;
#endif
        seen[txID] = true;
        if (myTxID == txID) {
            finished[txID] = true;
            return invisible ? true : false;
        }

        if (g.at(txID).is_aborted) {
            finished[txID] = true;
            return false;
        }

        TxSet edges(g.at(txID).readBy_);
        edges.insert(g.at(txID).writtenBy_.begin(), g.at(txID).writtenBy_.end());
#if DEBUG_MSG
        std::cout << "      Next: ";
        for (const auto& next : edges)
            std::cout << next << " ";
        std::cout << std::endl;
#endif

        bool overwrites; // whether if this tx overwrites target key
        if (auto it = g.at(txID).writeSet_.find(key); it != g.at(txID).writeSet_.end()
            && txID != version) // note that we can read it if txID == version
            overwrites = true;
        else
            overwrites = invisible || false;

        for (const auto& next : edges) {
            if (auto it = g.find(next); it == g.end()) // TODO: why is this check necessary?
                continue;
            auto f = finished.find(next);
            if (f != finished.end()) {
                continue;
            }
            auto s = seen.find(next);
            if (s != seen.end() && seen[next]
                && (f == finished.end() || !finished[next])) continue; // cycle
            // stack.push_back(txID);
            auto ret = is_invisible_dfs(key, version, myTxID, g, next, overwrites, seen, finished, stack);
            // stack.pop_back();
            if (ret) {
                finished[txID] = true;
                return ret;
            }
        }
#if DEBUG_MSG
        std::cout << "    Checking TxID " << txID << " done with false" << std::endl;
#endif
        finished[txID] = true;
        return false;
    }

    bool is_invisible(Key key, TxID version, TxID myTxID, Graph &g) {
        // check if a key:version pair is visible to me (myTxID)
        TxID start = version;
        std::map<TxID,bool> seen, finished;
        std::vector<TxID> stack; // for debug use
        return is_invisible_dfs(key, version, myTxID, g, start, false, seen, finished, stack);
    }

    Version* get_a_version(Version* ver, std::vector<Version*>& followers) {
        while (ver->status_.load(memory_order_acquire) != VersionStatus::committed
            && ver->status_.load(memory_order_acquire) != VersionStatus::deleted) {
            if (ver->status_.load(memory_order_acquire) == VersionStatus::aborted) {
#if DEBUG_MSG
                std::cout << "    Skip aborted version: " << ver->txid_ << std::endl;
#endif
                ver = ver->ldAcqNext();
            } else if (ver->status_.load(memory_order_acquire) == VersionStatus::pending
                || ver->status_.load(memory_order_acquire) == VersionStatus::deleting) {
#if DEBUG_MSG
                std::cout << "    Skip pending version: " << ver->txid_ << std::endl;
#endif
                followers.push_back(ver);
                ver = ver->ldAcqNext();
            }
            if (ver == VERSION_TERMINATION) {
#if DEBUG_MSG
                std::cout << "    Version does not exists" << std::endl;
#endif
                return ver;
            }
            if (ver == nullptr) {
#if DEBUG_MSG
                std::cout << "    No readable version" << std::endl;
#endif
                return ver;
            }
        }
        return ver;
    }

    void update_read_from(Tuple* tuple, Version* ver) {
        // TODO: do not remove TxNode if the version still alive?
        if (auto it = tuple->graph_.find(ver->txid_); it == tuple->graph_.end()) {
#if DEBUG_MSG
            std::cout << "  TxNode for version " << ver->txid_ << " not found" << std::endl;
#endif
            tuple->graph_.emplace(ver->txid_, TxNode(ver->txid_));
        }
        // add candidate edge
        tuple->graph_.at(ver->txid_).readBy_.emplace(this->txid_);
    }

    bool has_inflight_reader(Tuple* tuple, TxID txid) {
        if (auto itr1 = tuple->graph_.find(txid); itr1 != tuple->graph_.end()) {
            for (auto& t : tuple->graph_.at(txid).readBy_) {
                if (t == this->txid_) continue;
                if (auto itr2 = tuple->graph_.find(t); itr2 != tuple->graph_.end()) {
                    if (tuple->graph_.at(t).status_ == TransactionStatus::inflight) {
                        if (auto itr3 = tuple->graph_.at(t).readSet_.find(tuple);
                            itr3 != tuple->graph_.at(t).readSet_.end()) {
#if DEBUG_MSG
                            std::cout << "  Inflight reader: " << t << std::endl;
#endif
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

    Status __get_visible_version(Tuple *tuple, Version **return_ver, bool aligned_read=false) {
        Status stat = Status::OK;
        bool isInvisible;
        Version* ver = nullptr;
        Version* prev_ver = nullptr;
        std::vector<Version*> followers;
        ver = tuple->ldAcqLatest();
RETRY:
        followers.clear();
#if DEBUG_MSG
        std::cout << "  Selecting read targets" << std::endl;
#endif

#if WAIT_PENDING_VERSION
        // TODO: to wait or not to wait
        while (ver->status_.load(memory_order_acquire) != VersionStatus::committed
            && ver->status_.load(memory_order_acquire) != VersionStatus::deleted) {
            while (ver->status_.load(memory_order_acquire) == VersionStatus::pending
                || ver->status_.load(memory_order_acquire) == VersionStatus::deleting) {
                // To proceed other's commit we have to release lock of this page once
                tuple->lock_.w_unlock();
                sleepTics(100);
                tuple->lock_.w_lock();
            }
            if (ver->status_.load(memory_order_acquire) == VersionStatus::aborted) {
                ver = ver->ldAcqNext();
            }
            if (ver == nullptr) {
                stat = Status::ERROR_NO_VISIBLE_VERSION;
                goto OUT;
            }
        }
#else
        ver = get_a_version(ver, followers);
        if (ver == VERSION_TERMINATION) {
            // versions exist but no committed ones
            stat = Status::WARN_NOT_FOUND;
            goto OUT;
        }
        if (ver == nullptr) {
            // committed versions exist but no visible ones
            stat = Status::ERROR_NO_VISIBLE_VERSION;
            goto OUT;
        }
#endif

        if (aligned_read) {
            prev_ver = ver->ldAcqNext();
            if (prev_ver && has_inflight_reader(tuple, prev_ver->txid_)) {
#if DEBUG_MSG
                std::cout << "  Inflight reader found, drop " << ver->txid_ << std::endl;
#endif
                followers.push_back(ver);
                ver = prev_ver;
            } else {
                aligned_read = false;
            }
        }

#if DEBUG_MSG
            std::cout << "  Version " << ver->txid_
                      << " found, followers size is " << followers.size() << std::endl;
#endif
        if (reconnoitering_) {
          goto OUT;
        }

        // add anti-dependency edges found when skipping versions
        for (auto& follower : followers) {
            if (follower->txid_.epoch <  this->txid_.epoch) {
                stat = Status::ERROR_NO_VISIBLE_VERSION;
                goto OUT;
            }
            tuple->graph_.at(this->txid_).writtenBy_.emplace(follower->txid_);
        }

        update_read_from(tuple, ver);

        if (has_cycle(tuple->graph_, this->txid_)) {
#if DEBUG_MSG
            std::cout << "  Version " << ver->txid_ << " is invisible" << std::endl;
#endif
            // remove candidate edge and try next candidate
            tuple->graph_.at(ver->txid_).readBy_.erase(this->txid_);

            if (aligned_read) {
                // retry just once to check if the latest is visible
                followers.clear();
                ver = get_a_version(tuple->ldAcqLatest(), followers);
                if (ver == nullptr) {
                    stat = Status::ERROR_NO_VISIBLE_VERSION;
                    goto OUT;
                }
                update_read_from(tuple, ver);
                if (is_invisible(tuple, ver->txid_, this->txid_, tuple->graph_)) {
                    stat = Status::ERROR_NO_VISIBLE_VERSION;
                }
                goto OUT;
            }

            if (ver->txid_.epoch < this->txid_.epoch) {
                stat = Status::ERROR_NO_VISIBLE_VERSION;
                goto OUT;
            }

            // add anti-dependency edge before retry
            tuple->graph_.at(this->txid_).writtenBy_.emplace(ver->txid_);
            ver = ver->ldAcqNext();
            if (ver == VERSION_TERMINATION) {
#if DEBUG_MSG
                std::cout << "    Version does not exists" << std::endl;
#endif
                stat = Status::WARN_NOT_FOUND;
                goto OUT;
            }
            if (ver == nullptr) {
#if DEBUG_MSG
                std::cout << "  No readable version" << std::endl;
#endif
                stat = Status::ERROR_NO_VISIBLE_VERSION;
                goto OUT;
            } else {
#if DEBUG_MSG
                std::cout << "  Retry to check next version" << std::endl;
#endif
                goto RETRY;
            }
        }
OUT:
        *return_ver = ver;
        return stat;
    }

    uint64_t get_read_version_cardinality() {
        // only for statistics
        TxSet txns;
        auto itr = read_set_.begin();
        for (auto re : read_set_) {
            txns.emplace(re.txid_);
        }
        return txns.size();
    }

    uint64_t get_first_read_epoch() {
        auto itr = read_set_.begin();
        if (itr != read_set_.end()) {
            TxID txid = (*itr).txid_;
            return txid.epoch;
        }
        return -1;
    }

    Status get_visible_version(Tuple *tuple, Version **return_ver) {
#ifdef USE_FIRST_READ_EPOCH
        uint64_t epoch = get_first_read_epoch();
        if (epoch >= 0) {
            auto itr1 = tuple->version_index_.find(epoch);
            if (itr1 != tuple->version_index_.end()) {
                for (auto& ver : tuple->version_index_.at(epoch)) {
                    if (ver->status_.load(memory_order_acquire) == VersionStatus::committed
                        || ver->status_.load(memory_order_acquire) == VersionStatus::deleted) {
                        if (auto itr2 = tuple->graph_.find(ver->txid_); itr2 == tuple->graph_.end()) {
                            tuple->graph_.emplace(ver->txid_, TxNode(ver->txid_));
                        }
                        // add candidate edge
                        tuple->graph_.at(ver->txid_).readBy_.emplace(this->txid_);
                        if (!is_invisible(tuple, ver->txid_, this->txid_, tuple->graph_)) {
                            *return_ver = ver;
                            return Status::OK;
                        } else {
                            tuple->graph_.at(ver->txid_).readBy_.erase(this->txid_);
                        }
                    }
                }
            }
        }
        // fall back if committed visible version not found using index
#endif
        return __get_visible_version(tuple, return_ver);
    }

    Status get_aligned_visible_version(Tuple* tuple, Version** return_ver) {
        return __get_visible_version(tuple, return_ver, true);
    }

    Status get_visible_version_occ(Tuple* tuple, Version** return_ver) {
      Version *ver;
      Status stat = Status::OK;

      ver = tuple->ldAcqLatest();
      while (ver->status_.load(memory_order_acquire) != VersionStatus::committed
             && ver->status_.load(memory_order_acquire) != VersionStatus::deleted) {
        /**
         * Wait for the result of the pending version in the view.
         */
        while (ver->status_.load(memory_order_acquire) == VersionStatus::pending) {
        }
        if (ver->status_.load(memory_order_acquire) == VersionStatus::aborted) {
          ver = ver->ldAcqNext();
        }
        if (ver->status_ == VersionStatus::deleted) {
          stat = Status::WARN_NOT_FOUND;
          goto OUT;
        }
        if (ver == VERSION_TERMINATION) {
          // versions exist but no committed ones
          stat = Status::WARN_NOT_FOUND;
          goto OUT;
        }
        if (ver == nullptr) {
          // committed versions exist but no visible ones
          stat = Status::ERROR_NO_VISIBLE_VERSION;
          goto OUT;
        }
      }
      *return_ver = ver;

OUT:
      return stat;
    }

    void add_related_read_keys(const TxNode *tx, KeySet &keys) {
        for (const auto& [key, id] : tx->readSet_)
            keys.insert(key);
    }

    void add_related_all_keys(const TxNode *tx, KeySet &keys) {
        for (const auto& [key, id] : tx->readSet_)
            keys.insert(key);
        for (const auto& key : tx->writeSet_)
            keys.insert(key);
    }

    void push_candidate_keys(std::vector<Key> &targets, std::vector<Key> &done, KeySet &keys) {
        for (const auto& key : keys) {
            auto result1 = std::find(targets.begin(), targets.end(), key);
            auto result2 = std::find(done.begin(), done.end(), key);
            if (result1 == targets.end() && result2 == done.end())
                targets.push_back(key);
        }
    }

    void push_candidate_keys(KeySet &targets, KeySet &done, KeySet &keys) {
        for (const auto& key : keys) {
            auto result1 = targets.find(key);
            auto result2 = done.find(key);
            if (result1 == targets.end() && result2 == done.end())
                targets.insert(key);
        }
    }

    bool check_rf_cycle_dfs(Graph &g, TxSet &readsfrom, std::map<TxID,bool> &seen, TxID txID) {
        seen.emplace(txID, true);
        TxSet edges(g.at(txID).readBy_);
        edges.insert(g.at(txID).writtenBy_.begin(), g.at(txID).writtenBy_.end());

        if (auto it = readsfrom.find(txID); it != readsfrom.end())
            return true;

        for (const auto& next : edges) {
            if (auto it = seen.find(next); it != seen.end() && seen[next]) continue;
            auto ret = check_rf_cycle_dfs(g, readsfrom, seen, next);
            if (ret)
                return ret;
        }
        return false;
    }

    bool has_reads_from_cycle(TxID start, Graph &g, ReadSet &readSet) {
        std::map<TxID,bool> seen;
        TxSet readsfrom;
        for (const auto& [k, id] : readSet) {
            readsfrom.insert(id);
        }
        for (const auto& next : g.at(start).writtenBy_) {
            auto ret = check_rf_cycle_dfs(g, readsfrom, seen, next);
            if (ret) return true;
        }
        return false;
    }

    bool check_cycle_dfs(const Graph &g, std::map<TxID,bool> &seen, std::map<TxID,bool> &finished, const TxID txID) {
        if (quit_) {
            dump(thid_, "WARN: Quit cycle check because benchmark has finished.");
            status_ = TransactionStatus::invalid;
            return true; // TODO: should return with INVALID
        }
        seen.emplace(txID, true);
        TxSet edges(g.at(txID).readBy_);
        edges.insert(g.at(txID).writtenBy_.begin(), g.at(txID).writtenBy_.end());

        for (const auto& next : edges) {
            auto f = finished.find(next);
            if (f != finished.end() && finished[next]) continue;
            auto s = seen.find(next);
            if (s != seen.end() && seen[next] && f == finished.end())
                return true;
            if (auto it = g.find(next); it != g.end()) {
                auto ret = check_cycle_dfs(g, seen, finished, next);
                if (ret)
                    return ret;
            }
        }
        finished[txID] = true;
        return false;
    }

    bool has_cycle(const Graph &g, const TxID start) {
#if ADD_ANALYSIS
        result_->local_graph_size_ += g.size();
        ++result_->local_cycle_check_count_;
#endif  // if ADD_ANALYSIS
        std::map<TxID,bool> seen, finished;
        TxSet edges(g.at(start).readBy_);
        edges.insert(g.at(start).writtenBy_.begin(), g.at(start).writtenBy_.end());

        for (const auto& next : edges) {
            if (auto it = g.find(next); it != g.end()) {
                auto ret = check_cycle_dfs(g, seen, finished, next);
                if (ret) return true;
            }
        }
        return false;
    }

    // probably only debug-use
    bool has_cycle(Graph &g) {
        std::map<TxID,bool> seen, finished;
        for (const auto& [txid, node] : g) {
            TxSet edges(g.at(txid).readBy_);
            edges.insert(g.at(txid).writtenBy_.begin(), g.at(txid).writtenBy_.end());
            for (const auto& next : edges) {
                auto ret = check_cycle_dfs(g, seen, finished, next);
                if (ret) return true;
            }
        }
        return false;
    }

    void merge(Graph &target, Graph &source) {
        for (const auto& [tid, sourceTxNode] : source) {
            TxNode *targetTxNode;
            if (auto it = target.find(tid); it != target.end()) {
                targetTxNode = &target.at(tid);
            } else {
                target.emplace(tid, TxNode(tid));
                targetTxNode = &target.at(tid);
            }
            if (targetTxNode->is_aborted) {
                clean_up_node_edges(target, tid);
                continue;
            }
            targetTxNode->readSet_.insert(sourceTxNode.readSet_.begin(), sourceTxNode.readSet_.end());
            targetTxNode->writeSet_.insert(sourceTxNode.writeSet_.begin(), sourceTxNode.writeSet_.end());
            for (const auto& to : sourceTxNode.readBy_)
                targetTxNode->readBy_.insert(to);
            for (const auto& to : sourceTxNode.writtenBy_)
                targetTxNode->writtenBy_.insert(to);
            for (const auto& from : sourceTxNode.from_)
                targetTxNode->from_.insert(from);
            // Update status if the transaction in source graph is validating
            if (targetTxNode->status_ ==  TransactionStatus::inflight
                && sourceTxNode.status_ ==  TransactionStatus::validating)
                targetTxNode->status_ = TransactionStatus::validating;
        }
    }

    uint64_t get_min_epoch() {
        uint64_t min = UINT64_MAX;
        for (unsigned int i = 0; i < TotalThreadNum; ++i) {
            auto e = ThManagementTable[i]->atomicLoadEpoch();
            if (e < min)
                min = e;
        }
        return min;
    }

    void erase_following_edges(Graph &g, const TxID txid) {
        for (auto& reader : g.at(txid).readBy_) {
            if (auto it = g.find(reader); it != g.end()) // TODO: why is this check necessary?
                g.at(reader).from_.erase(txid);
        }
        for (auto& writer : g.at(txid).writtenBy_) {
            if (auto it = g.find(writer); it != g.end()) // TODO: why is this check necessary?
                g.at(writer).from_.erase(txid);
        }
    }

    void gc(Graph &g, uint64_t epoch) {
        bool assert = false;
        int non = 0;
        int incoming = 0;
        int read = 0;
        int write = 0;
        // if (g.size() > 50) {
        //     generate_dot_graph("gc1", g);
        //     assert = true;
        // }
#if DEBUG_MSG
        std::cout << "  GC check start, reclamation epoch = " << epoch << std::endl;
#endif
        for (auto it = g.begin(); it != g.end();) {
            auto txid = (*it).first;
            auto txNode = &(*it).second;
#ifdef GC_ABORTED_TX
            if (is_aborted(txid)) {
                // std::cout << "  Aborted TxNode: " << txid << " removed in GC" << std::endl;
                clean_up_node_edges(g, txid);
                it = g.erase(it);
                continue;
            }
#endif
            if (txid.epoch <= epoch) {
                // std::cout << "TxID: " << txid << std::endl;
                bool keep = false;
                for (auto i = txNode->readBy_.begin(); i != txNode->readBy_.end();) {
                    auto reader = (*i);
                    if (reader.epoch <= epoch) {
                        // std::cout << " R " << reader << std::endl;
                        if (auto it = g.find(reader); it != g.end()) // TODO: why is this check necessary?
                            g.at(reader).from_.erase(txid);
                        i = txNode->readBy_.erase(i);
                    } else {
                        keep = true;
                        read++;
                        ++i;
                    }
                }
                for (auto i = txNode->writtenBy_.begin(); i != txNode->writtenBy_.end();) {
                    auto writer = (*i);
                    if (writer.epoch <= epoch) {
                        // std::cout << " W " << writer << std::endl;
                        if (auto it = g.find(writer); it != g.end()) // TODO: why is this check necessary?
                            g.at(writer).from_.erase(txid);
                        i = txNode->writtenBy_.erase(i);
                    } else {
                        keep = true;
                        write++;
                        ++i;
                    }
                }
                for (auto i = txNode->from_.begin(); i != txNode->from_.end();) {
                    auto from = (*i);
                    if (from.epoch > epoch) {
                        keep = true;
                        break;
                    }
                    ++i;
                }

                // std::cout << " -> R: ";
                // for (const auto& id : txNode->readBy_) std::cout << id << " "; std::cout << std::endl;
                // std::cout << " -> W: ";
                // for (const auto& id : txNode->writtenBy_) std::cout << id << " "; std::cout << std::endl;
                if (keep) {
                    ++it;
                    continue;
                }
                if (txNode->from_.size() > 0) {
                    ++it;
                    incoming++;
                    continue;
                }
                // std::cout << "  Reclaim TxID = " << txid
                //           << ", Reclaimation epoch = " << epoch
                //           << ", Local epoch = " << ThLocalEpoch[thid_].obj_
                //           << ")" << std::endl;
                erase_following_edges(g, txid);
                it = g.erase(it);
            }
            else {
                non++;
                ++it;
            }
        }
        if (assert) {
            std::cout << "GC done" << std::endl;
            std::cout << "Reclaimation epoch: " << epoch << std::endl;
            std::cout << "Local epoch: " << ThManagementTable[thid_]->atomicLoadEpoch() << std::endl;
            std::cout << "Non: " << non << std::endl;
            std::cout << "Incoming: " << incoming << std::endl;
            std::cout << "Read: " << read << std::endl;
            std::cout << "Write: " << write << std::endl;
            generate_dot_graph("gc2", g);
            my_assert(false);
        }
    }

    void remove_indifferent_nodes(Graph &g) {
        auto n = 0;
        auto m = 0;
        TxSet reachable_from;
        TxSet reachable_to;
        find_reachable_from(this->txid_, g, reachable_from);
        find_reachable_to(this->txid_, g, reachable_to);
        for (auto it = g.begin(); it != g.end();) {
            auto txid = (*it).first;
            auto txnode = (*it).second;
            auto conflict = has_any_conflict(
                                this->readSet_, this->writeSet_,
                                g.at(txid).readSet_,
                                g.at(txid).writeSet_);
            if (!conflict) {
                n++;
                auto it1 = reachable_from.find(txid);
                auto it2 = reachable_to.find(txid);
                if (it1 == reachable_from.end() && it2 == reachable_to.end()) {
                    for (auto& from : g.at(txid).from_) {
                        g.at(from).readBy_.erase(txid);
                        g.at(from).writtenBy_.erase(txid);
                    }
                    erase_following_edges(g, txid);
                    it = g.erase(it);
                    m++;
                    continue;
                }
            }
            ++it;
        }
        // std::cout << "Non-conflict nodes: " << n << std::endl;
        // std::cout << "Indifferent nodes: " << m << std::endl;
    }

    TxID get_read_version(Key key) {
        auto it = readSet_.find(key);
        if (it != readSet_.end())
            return it->second;
        else
            return TxID();
    }

    Status insert_version(WriteElement<Tuple> we) {
         // For insert record, followers set is always empty due to post-ordering only
        TxSet s;
        return insert_version(we, s);
    }

    Status insert_version(WriteElement<Tuple> we, TxSet& followers) {
        Tuple* tuple = we.rcdptr_;
        if (we.op_ == OpType::DELETE) {
            we.new_ver_->status_ = VersionStatus::deleting;
        } else {
            we.new_ver_->status_ = VersionStatus::pending;
        }
        Version* v = tuple->latest_;
        if (v->status_ == VersionStatus::deleting || v->status_ == VersionStatus::deleted) {
            return Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
        }
        if (followers.size() == 0) {
            we.new_ver_->next_ = v;
            tuple->latest_ = we.new_ver_;
        } else {
            Version* prev = v;
            while (followers.size() > 0) {
                followers.erase(v->txid_);
                prev = v;
                v = v->next_;
            }
            prev->next_ = we.new_ver_;
            we.new_ver_->next_ = v;
        }
        return Status::OK;
    }

    void clean_up_node_edges(Graph &g, const TxID txid) {
        // TODO: can be sophisticated?
        for (const auto& to : g.at(txid).writtenBy_) {
            if (auto it = g.find(to); it != g.end()) // TODO: can be checkless?
                g.at(to).from_.erase(txid);
        }
        for (const auto& from : g.at(txid).from_) {
            if (auto it = g.find(from); it != g.end()) { // TODO: can be checkless?
                g.at(from).readBy_.erase(txid);
                g.at(from).writtenBy_.erase(txid);
            }
        }
        // We want to just remove this aborted node,
        // but some other worker may have seen this node
        // (that is still inflight at that time), and try to merge
        // without knowing it has already aborted.
        // So, just mark it aborted and prevent it from adding edges
        g.at(txid).readBy_.clear();
        g.at(txid).writtenBy_.clear();
        g.at(txid).from_.clear();
        g.at(txid).is_aborted = true;
    }

    void lock_write_set() {
      for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if (itr->op_ == OpType::INSERT) continue;
        itr->rcdptr_->lock_.w_lock();
        if (IS_OCC(cc_mode_) && itr->op_ == OpType::UPDATE) {
          // in Oze mode, we already inserted a version with concurrent deletion check,
          // so we can skip the check below
          Version* ver;
          Status stat = get_visible_version_occ(itr->rcdptr_, &ver);
          if (stat != Status::OK || ver->status_ == VersionStatus::deleted) {
            unlock_write_set(itr);
            this->status_ = TransactionStatus::aborted;
            return;
          }
        }

        this->max_tid_wset_ = max(this->max_tid_wset_, (*itr).rcdptr_->tuple_id_);
      }
    }

    void unlock_write_set() {
      for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        (*itr).rcdptr_->lock_.w_unlock();
      }
    }

    void unlock_write_set(std::vector<WriteElement<Tuple>>::iterator end) {
      for (auto itr = write_set_.begin(); itr != end; ++itr) {
        if ((*itr).op_ == OpType::INSERT) continue;
        (*itr).rcdptr_->lock_.w_unlock();
      }
    }

    TupleId decide_tuple_id() {
      TupleId tid_a, tid_b, tid_c;

      // calculates (a)
      // about read_set_
      tid_a = std::max(max_tid_wset_, max_tid_rset_);
      tid_a.tid++;

      // calculates (b)
      // larger than the worker's most recently chosen TID,
      tid_b = most_recent_tid_;
      tid_b.tid++;

      // calculates (c)
      tid_c.epoch = txid_.epoch;

      // compare a, b, c
      return std::max({tid_a, tid_b, tid_c});
    }

    void commit_version(WriteElement<Tuple> we) {
      if (we.op_ == OpType::DELETE) {
        Masstrees[get_storage(we.storage_)].remove_value(we.key_);
        we.new_ver_->status_.store(VersionStatus::deleted, std::memory_order_release);
        gc_records_.push_back(we.rcdptr_);
      } else {
        we.new_ver_->status_.store(VersionStatus::committed, std::memory_order_release);
      }
    }

    bool should_switch_to_occ() {
      // TODO: implement
      // if (result_->local_commit_counts_ > 100) {
      //  return true;
      // }
      return false;
    }

    bool should_switch_to_oze() {
      // TODO: implement
      // if (result_->local_commit_counts_ > 5000) {
      //   return true;
      // }
      return false;
    }

    uint8_t get_next_cc_mode() {
      uint8_t mode_array[TotalThreadNum];
      for (unsigned int i = 0; i < TotalThreadNum; ++i) {
        mode_array[i] = ThManagementTable[i]->atomicLoadMode();
      }
      if (all_threads_in(OCC_MODE, mode_array)) {
        return should_switch_to_oze() ? TO_OZE_MODE : OCC_MODE;
      }
      if (all_threads_in(OZE_MODE, mode_array)) {
        return should_switch_to_occ() ? TO_OCC_MODE : OZE_MODE;
      }
      if (cc_mode_ == TO_OCC_MODE
          && some_other_threads_in(OZE_MODE, mode_array, thid_)) {
        return TO_OCC_MODE;
      }
      if (cc_mode_ == TO_OZE_MODE
          && some_other_threads_in(OCC_MODE, mode_array, thid_)) {
        return TO_OZE_MODE;
      }
      if (some_threads_in(TO_OCC_MODE, mode_array)) {
        return OCC_MODE;
      }
      if (some_threads_in(TO_OZE_MODE, mode_array)) {
        return OZE_MODE;
      }
      ERR;
    }

    void print_read_write_set(ReadSet rset, WriteSet wset) {
        std::cout << "    ReadSet: ";
        for (const auto& [key, id] : rset)
            std::cout << key << "-" << id << " ";
        std::cout << std::endl;
        std::cout << "    WriteSet: ";
        for (auto& key : wset)
            std::cout << key << " ";
        std::cout << std::endl;
    }

    void print_graph(Graph g) {
        std::cout << "  Graph: " << std::endl;
        for (const auto& [txid, node] : g) {
            std::cout << "    TxID: " << txid;
            if (node.is_aborted) {
                std::cout << " (a)" << std::endl;
            } else {
                std::cout << std::endl;
            }
            print_read_write_set(node.readSet_, node.writeSet_);
            std::cout << "      Read by: ";
            for (const auto& id : node.readBy_)
                std::cout << id << " ";
            std::cout << std::endl;
            std::cout << "      Written by: ";
            for (const auto& id : node.writtenBy_)
                std::cout << id << " ";
            std::cout << std::endl;
            std::cout << "      From: ";
            for (const auto& id : node.from_)
                std::cout << id << " ";
            std::cout << std::endl;
        }
    }

    void generate_dot_graph(std::string name, Graph &g) {
        std::string thid = "th" + std::to_string(thid_);
        std::string dotfile = DEBUG_OUT_DIR + thid + name + ".dot";
        std::string pngfile = DEBUG_OUT_DIR + thid + name + ".png";
        std::ofstream ofs(dotfile);
        ofs << "digraph " << name << " {" << std::endl;
        ofs << "  graph [rankdir=LR];" << std::endl;
        ofs << "  node  [shape=circle];" << std::endl;
        for (const auto& [txid, node] : g) {
            for (const auto& id : node.readBy_) {
                ofs << " \"" << txid << "\" -> \"" << id << "\";" << std::endl;
            }
            for (const auto& id : node.writtenBy_) {
                ofs << " \"" << txid << "\" -> \"" << id << "\"[color=red];" << std::endl;
            }
        }
        ofs << "}" << std::endl;
        ofs.close();
        // std::string options = " -T png -o " + pngfile + " " + dotfile;
        // std::string command = DOT_COMMAND + options;
        // auto result = system(command.c_str());
    }

    void print_record(Tuple* tuple) {
        if (!tuple) {
            std::cout << "No such record." << std::endl;
            return;
        }
        std::string key(tuple->latest_.load(memory_order_acquire)->body_.get_key());
        std::cout << "Key: " << key << " (" << tuple << ")" << std::endl;
        print_graph(tuple->graph_);
    }

    void print_transaction() {
        std::cout << "TxID: " << txid_ << " " << std::endl;
        print_read_write_set(readSet_, writeSet_);
        print_graph(graph_);
    }

    std::string to_string() {
        return std::to_string(txid_.epoch) + "_"
                + std::to_string(txid_.thid) + "_" + std::to_string(txid_.tid);
    }
};
