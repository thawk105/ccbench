
#include <string.h>
#include <atomic>
#include <algorithm>
#include <bitset>

#include "../include/atomic_wrapper.hh"
#include "../include/debug.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/tsc.hh"
#include "include/common.hh"
#include "include/transaction.hh"
#include "include/version.hh"

extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);

using namespace std;

inline
SetElement<Tuple> *
TxExecutor::searchReadSet(unsigned int key) 
{
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

inline
SetElement<Tuple> *
TxExecutor::searchWriteSet(unsigned int key) 
{
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    if ((*itr).key_ == key) return &(*itr);
  }

  return nullptr;
}

void
TxExecutor::tbegin()
{
  TransactionTable *newElement, *tmt;

  tmt = loadAcquire(TMT[thid_]);
  if (this->status_ == TransactionStatus::aborted) {
    this->txid_ = tmt->lastcstamp_.load(memory_order_acquire);
    newElement = new TransactionTable(0, 0, UINT32_MAX, tmt->lastcstamp_.load(std::memory_order_acquire), TransactionStatus::inFlight);
  }
  else {
    this->txid_ = this->cstamp_;
    newElement = new TransactionTable(0, 0, UINT32_MAX, tmt->cstamp_.load(std::memory_order_acquire), TransactionStatus::inFlight);
  }

  for (unsigned int i = 1; i < THREAD_NUM; ++i) {
    if (i == thid_) continue;
    do {
      tmt = loadAcquire(TMT[i]);
    } while (tmt == nullptr);
    this->txid_ = max(this->txid_, tmt->lastcstamp_.load(memory_order_acquire));
  }
  this->txid_ += 1;
  newElement->txid_ = this->txid_;

  TransactionTable *expected, *desired;
  tmt = loadAcquire(TMT[thid_]);
  expected = tmt;
  gcobject_.gcq_for_TMT_.push(expected);
  for (;;) {
    desired = newElement;
    if (compareExchange(TMT[thid_], expected, desired)) break;
  }
  
  pstamp_ = 0;
  sstamp_ = UINT32_MAX;
  status_ = TransactionStatus::inFlight;
}

char *
TxExecutor::ssn_tread(unsigned int key)
{
  //safe retry property
  
  //if it already access the key object once.
  // w
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return writeVal;
  // tanabe の実験ではスレッドごとに書き込む新しい値が決まっている．

  SetElement<Tuple> *inR = searchReadSet(key);
  if (inR) {
    return inR->ver_->val_;
  }

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
  #if ADD_ANALYSIS
    ++eres_->local_tree_traversal_;
  #endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  // if v not in t.writes:
  Version *ver = tuple->latest_.load(std::memory_order_acquire);
  if (ver->status_.load(memory_order_acquire) != VersionStatus::committed) {
    ver = ver->committed_prev_;
  }
  uint64_t verCstamp = ver->cstamp_.load(memory_order_acquire);
  while (txid_ < (verCstamp >> TIDFLAG)) {
    //printf("txid %d, (verCstamp >> 1) %d\n", txid, verCstamp >> 1);
    //fflush(stdout);
    ver = ver->committed_prev_;
    if (ver == NULL) {
      cout << "txid : " << txid_
        << ", verCstamp : " << verCstamp << endl;
      ERR;
    }
    verCstamp = ver->cstamp_.load(memory_order_acquire);
  }

  if (ver->psstamp_.atomicLoadSstamp() == (UINT32_MAX & ~(TIDFLAG)))
    // no overwrite yet
    readSet_.emplace_back(key, tuple, ver);
  else 
    // update pi with r:w edge
    this->sstamp_ = min(this->sstamp_, (ver->psstamp_.atomicLoadSstamp() >> TIDFLAG));
  upReadersBits(ver);

  verify_exclusion_or_abort();

  // for fairness
  // ultimately, it is wasteful in prototype system.
  memcpy(returnVal, ver->val_, VAL_SIZE);
  return ver->val_;
}

void
TxExecutor::ssn_twrite(unsigned int key)
{
  // if it already wrote the key object once.
  SetElement<Tuple> *inW = searchWriteSet(key);
  if (inW) return;
  // tanabe の実験では，スレッドごとに書き込む新しい値が決まっている．
  // であれば，コミットが確定し，version status を committed にして other worker に visible になるときに
  // memcpy するのが一番無駄が少ない．

  // if v not in t.writes:
  //
  //first-updater-wins rule
  //Forbid a transaction to update  a record that has a committed head version later than its begin timestamp.
  
  Version *expected, *desired;
  desired = new Version();
  uint64_t tmptid = this->thid_;
  tmptid = tmptid << 1;
  tmptid |= 1;
  desired->cstamp_.store(tmptid, memory_order_relaxed);  // read operation, write operation, ガベコレからアクセスされるので， CAS 前に格納

#if MASSTREE_USE
  Tuple *tuple = MT.get_value(key);
  #if ADD_ANALYSIS
    ++eres_->local_tree_traversal_;
  #endif
#else
  Tuple *tuple = get_tuple(Table, key);
#endif

  Version *vertmp;
  expected = tuple->latest_.load(std::memory_order_acquire);
  for (;;) {
    // w-w conflict
    // first updater wins rule
    if (expected->status_.load(memory_order_acquire) == VersionStatus::inFlight) {
      this->status_ = TransactionStatus::aborted;
      TransactionTable *tmt = loadAcquire(TMT[thid_]);
      tmt->status_.store(TransactionStatus::aborted, memory_order_release);
      delete desired;
      return;
    }
    
    // 先頭バージョンが committed バージョンでなかった時
    vertmp = expected;
    while (vertmp->status_.load(memory_order_acquire) != VersionStatus::committed) {
      vertmp = vertmp->committed_prev_;
      if (vertmp == nullptr) ERR;
    }

    // vertmp は commit済み最新バージョン
    uint64_t verCstamp = vertmp->cstamp_.load(memory_order_acquire);
    if (txid_ < (verCstamp >> 1)) {  
      //  write - write conflict, first-updater-wins rule.
      // Writers must abort if they would overwirte a version created after their snapshot.
      this->status_ = TransactionStatus::aborted;
      TransactionTable *tmt = loadAcquire(TMT[thid_]);
      tmt->status_.store(TransactionStatus::aborted, memory_order_release);
      delete desired;
      return;
    }

    desired->prev_ = expected;
    desired->committed_prev_ = vertmp;
    if (tuple->latest_.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
  }

  //ver->committed_prev_->sstamp_ に TID を入れる
  uint64_t tmpTID = thid_;
  tmpTID = tmpTID << 1;
  tmpTID |= 1;
  desired->committed_prev_->psstamp_.atomicStoreSstamp(tmpTID);

  // update eta with w:r edge
  this->pstamp_ = max(this->pstamp_, desired->committed_prev_->psstamp_.atomicLoadPstamp());
  writeSet_.emplace_back(key, tuple, desired);
  
  //  avoid false positive
  auto itr = readSet_.begin();
  while (itr != readSet_.end()) {
    if ((*itr).key_ == key) {
      readSet_.erase(itr);
      downReadersBits(vertmp);
      break;
    } else itr++;
  }
  
  verify_exclusion_or_abort();
}

void
TxExecutor::ssn_commit()
{
  this->status_ = TransactionStatus::committing;
  TransactionTable *tmt = loadAcquire(TMT[thid_]);
  tmt->status_.store(TransactionStatus::committing);

  this->cstamp_ = ++Lsn;
  tmt->cstamp_.store(this->cstamp_, memory_order_release);
  
  // begin pre-commit
  SsnLock.lock();

  // finalize eta(T)
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    pstamp_ = max(pstamp_, (*itr).ver_->committed_prev_->psstamp_.atomicLoadPstamp());
  } 

  // finalize pi(T)
  sstamp_ = min(sstamp_, cstamp_);
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    uint32_t verSstamp = (*itr).ver_->psstamp_.atomicLoadSstamp();
    // if the lowest bit raise, the record was overwrited by other concurrent transactions.
    // but in serial SSN, the validation of the concurrent transactions will be done after that of this transaction.
    // So it can skip.
    // And if the lowest bit raise, the stamp is TID;
    if (verSstamp & 1) continue;
    sstamp_ = min(sstamp_, verSstamp >> 1);
  }

  // ssn_check_exclusion
  if (pstamp_ < sstamp_) status_ = TransactionStatus::committed;
  else {
    status_ = TransactionStatus::aborted;
    SsnLock.unlock();
    return;
  }

  // update eta
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    (*itr).ver_->psstamp_.atomicStorePstamp(max((*itr).ver_->psstamp_.atomicLoadPstamp(), cstamp_));
    // down readers bit
    downReadersBits((*itr).ver_);
  }

  // update pi
  uint64_t verSstamp = this->sstamp_;
  verSstamp = verSstamp << 1;
  verSstamp &= ~(1);

  uint64_t verCstamp = cstamp_;
  verCstamp = verCstamp << 1;
  verCstamp &= ~(1);

  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    (*itr).ver_->committed_prev_->psstamp_.atomicStoreSstamp(verSstamp);
    // initialize new version
    (*itr).ver_->psstamp_.atomicStorePstamp(cstamp_);
    (*itr).ver_->cstamp_ = verCstamp;
  }

  //logging
  //?*
  
  // status, inFlight -> committed
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    memcpy((*itr).ver_->val_, writeVal, VAL_SIZE);
    (*itr).ver_->status_.store(VersionStatus::committed, memory_order_release);
  }

  this->status_ = TransactionStatus::committed;
  SsnLock.unlock();
  readSet_.clear();
  writeSet_.clear();
  return;
}

void
TxExecutor::ssn_parallel_commit()
{
  this->status_ = TransactionStatus::committing;
  TransactionTable *tmt = loadAcquire(TMT[thid_]);
  tmt->status_.store(TransactionStatus::committing);

  this->cstamp_ = ++Lsn;
  //this->cstamp_ = 2; test for effect of centralized counter in YCSB-C (read only workload)

  tmt->cstamp_.store(this->cstamp_, memory_order_release);
  
  // begin pre-commit
  
  // finalize pi(T)
  this->sstamp_ = min(this->sstamp_, this->cstamp_);
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    uint32_t v_sstamp = (*itr).ver_->psstamp_.atomicLoadSstamp();
    // if lowest bits raise, it is TID
    if (v_sstamp & TIDFLAG) {
      // identify worker by using TID
      uint8_t worker = (v_sstamp >> TIDFLAG);
      if (worker == thid_) {
        // it mustn't occur "worker == thid_", so it's error.
        dispWS();
        dispRS();
        ERR;
      }
      // もし worker が inFlight (read phase 実行中) だったら
      // このトランザクションよりも新しい commit timestamp でコミットされる．
      // 従って pi となりえないので，スキップ．
      // そうでないなら，チェック
      /*cout << "tmt : " << tmt << endl;
      cout << "worker : " << worker << endl;
      cout << "v_sstamp : " << v_sstamp << endl;
      cout << "UINT32_MAX : " << UINT32_MAX << endl;*/
      tmt = loadAcquire(TMT[worker]);
      if (tmt->status_.load(memory_order_acquire) == TransactionStatus::committing) {
        // worker は ssn_parallel_commit() に突入している
        // cstamp が確実に取得されるまで待機 (cstamp は初期値 0)
        while (tmt->cstamp_.load(memory_order_acquire) == 0);
        if (tmt->status_.load(memory_order_acquire) == TransactionStatus::aborted) continue;

        // worker->cstamp_ が this->cstamp_ より小さいなら pi になる可能性あり
        if (tmt->cstamp_.load(memory_order_acquire) < this->cstamp_) {
          // parallel_commit 終了待ち（sstamp確定待ち)
          while (tmt->status_.load(memory_order_acquire) == TransactionStatus::committing);
          if (tmt->status_.load(memory_order_acquire) == TransactionStatus::committed) {
            this->sstamp_ = min(this->sstamp_, tmt->sstamp_.load(memory_order_acquire));
          }
        }
      }
    } else {
      this->sstamp_ = min(this->sstamp_, v_sstamp >> TIDFLAG);
    }
  }

  // finalize eta
  uint64_t one = 1;
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {

    //for r in v.prev.readers
    uint64_t rdrs = (*itr).ver_->readers_.load(memory_order_acquire);
    for (unsigned int worker = 1; worker <= THREAD_NUM; ++worker) {
      if ((rdrs & (one << worker)) ? 1 : 0) {
        tmt = loadAcquire(TMT[worker]);
        // 並行 reader が committing なら無視できない．
        // inFlight なら無視できる．なぜならそれが commit する時にはこのトランザクションよりも
        // 大きい cstamp を有し， eta となりえないから．
        while (tmt->status_.load(memory_order_acquire) == TransactionStatus::committing) {
          while (tmt->cstamp_.load(memory_order_acquire) == 0);
          if (tmt->status_.load(memory_order_acquire) == TransactionStatus::aborted) continue;
          
          // worker->cstamp_ が this->cstamp_ より小さいなら eta になる可能性あり
          if (tmt->cstamp_.load(memory_order_acquire) < this->cstamp_) {
            // parallel_commit 終了待ち (sstamp 確定待ち)
            while (tmt->status_.load(memory_order_acquire) == TransactionStatus::committing);
            if (tmt->status_.load(memory_order_acquire) == TransactionStatus::committed) {
              this->pstamp_ = min(this->pstamp_, tmt->cstamp_.load(memory_order_acquire));
            }
          }
        }
      } 
    }
    // re-read pstamp in case we missed any reader
    this->pstamp_ = min(this->pstamp_, (*itr).ver_->committed_prev_->psstamp_.atomicLoadPstamp());
  }

  tmt = loadAcquire(TMT[thid_]);
  // ssn_check_exclusion
  if (pstamp_ < sstamp_) {
    status_ = TransactionStatus::committed;
    tmt->sstamp_.store(this->sstamp_, memory_order_release);
    tmt->status_.store(TransactionStatus::committed, memory_order_release);
  } else {
    status_ = TransactionStatus::aborted;
    tmt->status_.store(TransactionStatus::aborted, memory_order_release);
    return;
  }

  // update eta
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    Psstamp pstmp;
    pstmp.obj_ = (*itr).ver_->psstamp_.atomicLoad();
    while (pstmp.pstamp_ < this->cstamp_) {
      if ((*itr).ver_->psstamp_.atomicCASPstamp(pstmp.pstamp_, this->cstamp_)) break;
      pstmp.obj_ = (*itr).ver_->psstamp_.atomicLoad();
    }
    downReadersBits((*itr).ver_);
  }


  // update pi
  uint64_t verSstamp = this->sstamp_;
  verSstamp = verSstamp << TIDFLAG;
  verSstamp &= ~(TIDFLAG);

  uint64_t verCstamp = cstamp_;
  verCstamp = verCstamp << TIDFLAG;
  verCstamp &= ~(TIDFLAG);

  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    (*itr).ver_->committed_prev_->psstamp_.atomicStoreSstamp(verSstamp);
    (*itr).ver_->cstamp_.store(verCstamp, memory_order_release);
    (*itr).ver_->psstamp_.atomicStorePstamp(this->cstamp_);
    (*itr).ver_->psstamp_.atomicStoreSstamp(UINT32_MAX & ~(TIDFLAG));
    memcpy((*itr).ver_->val_, writeVal, VAL_SIZE);
    (*itr).ver_->status_.store(VersionStatus::committed, memory_order_release);
    gcobject_.gcq_for_version_.push(GCElement((*itr).key_, (*itr).rcdptr_, (*itr).ver_, verCstamp));
  }

  //logging
  //?*

  readSet_.clear();
  writeSet_.clear();
  ++eres_->local_commit_counts_;
  return;
}

void
TxExecutor::abort()
{
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    (*itr).ver_->committed_prev_->psstamp_.atomicStoreSstamp(UINT32_MAX & ~(TIDFLAG));
    (*itr).ver_->status_.store(VersionStatus::aborted, memory_order_release);
    gcobject_.gcq_for_version_.push(GCElement((*itr).key_, (*itr).rcdptr_, (*itr).ver_, this->txid_ << TIDFLAG));
  }
  writeSet_.clear();

  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr)
    downReadersBits((*itr).ver_);

  readSet_.clear();
  ++eres_->local_abort_counts_;
}

void
TxExecutor::verify_exclusion_or_abort()
{
  if (this->pstamp_ >= this->sstamp_) {
    this->status_ = TransactionStatus::aborted;
    TransactionTable *tmt = loadAcquire(TMT[thid_]);
    tmt->status_.store(TransactionStatus::aborted, memory_order_release);
  }
}

void
TxExecutor::mainte()
{
  gcstop = rdtscp();
  if (chkClkSpan(gcstart, gcstop, GC_INTER_US * CLOCKS_PER_US)) {
    uint32_t loadThreshold = gcobject_.getGcThreshold();
    if (pre_gc_threshold_ != loadThreshold) {
      gcobject_.gcTMTelement(eres_);
      gcobject_.gcVersion(eres_);
      pre_gc_threshold_ = loadThreshold;

#if ADD_ANALYSIS
      ++eres_->localGCCounts;
#endif
      gcstart = gcstop;
    }
  }
}

void
TxExecutor::dispWS()
{
  cout << "th " << this->thid_ << " : write set : ";
  for (auto itr = writeSet_.begin(); itr != writeSet_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
  }
  cout << endl;
}

void
TxExecutor::dispRS()
{
  cout << "th " << this->thid_ << " : read set : ";
  for (auto itr = readSet_.begin(); itr != readSet_.end(); ++itr) {
    cout << "(" << (*itr).key_ << ", " << (*itr).ver_->val_ << "), ";
  }
  cout << endl;
}

