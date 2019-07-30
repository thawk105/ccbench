#include <atomic>

#include "include/common.hh"
#include "include/lock.hh"

#define xchg(...) __atomic_exchange_n(__VA_ARGS__)
#define cas(...) __atomic_compare_exchange_n(__VA_ARGS__)
#define atoload(arg) __atomic_load_n(arg, __ATOMIC_ACQUIRE)
#define atostore(...) __atomic_store_n(__VA_ARGS__)

#define SPIN 2400

#ifdef MQLOCK
MQLMetaInfo MQLNode::atomicLoadSucInfo() {
  MQLMetaInfo load;
  load.obj = __atomic_load_n(&sucInfo.obj, __ATOMIC_ACQUIRE);
  return load;
}

bool MQLNode::atomicCASSucInfo(MQLMetaInfo expected, MQLMetaInfo desired) {
  return __atomic_compare_exchange_n(&sucInfo.obj, &expected.obj, desired.obj,
                                     false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

void MQLMetaInfo::init(bool busy, LockMode stype, LockStatus status,
                       uint32_t next) {
  this->busy = busy;
  this->stype = stype;
  this->status = status;
  this->next = next;
}

bool MQLMetaInfo::atomicLoadBusy() {
  MQLMetaInfo expected;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  return expected.busy;
}

void MQLMetaInfo::atomicStoreBusy(bool newbusy) {
  MQLMetaInfo expected, desired;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  for (;;) {
    desired = expected;
    desired.busy = newbusy;
    if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
      break;
  }
}

LockMode MQLMetaInfo::atomicLoadStype() {
  MQLMetaInfo expected;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  return expected.stype;
}

void MQLMetaInfo::atomicStoreStype(LockMode newlockmode) {
  MQLMetaInfo expected, desired;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  for (;;) {
    desired = expected;
    desired.stype = newlockmode;
    if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
      break;
  }
}

LockStatus MQLMetaInfo::atomicLoadStatus() {
  MQLMetaInfo expected;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  return expected.status;
}

void MQLMetaInfo::atomicStoreStatus(LockStatus newstatus) {
  MQLMetaInfo expected, desired;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  for (;;) {
    desired = expected;
    desired.status = newstatus;
    if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
      break;
  }
}

uint32_t MQLMetaInfo::atomicLoadNext() {
  MQLMetaInfo expected;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  return expected.next;
}

void MQLMetaInfo::atomicStoreNext(uint32_t newnext) {
  MQLMetaInfo expected, desired;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  for (;;) {
    desired = expected;
    desired.next = newnext;
    if (__atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false,
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
      break;
  }
}

bool MQLMetaInfo::atomicCASNext(uint32_t oldnext, uint32_t newnext) {
  MQLMetaInfo expected, desired;
  expected.obj = __atomic_load_n(&obj, __ATOMIC_ACQUIRE);
  expected.next = oldnext;
  desired = expected;
  desired.next = newnext;
  return __atomic_compare_exchange_n(&obj, &expected.obj, desired.obj, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

MQL_RESULT
MQLock::acquire_reader_lock(uint32_t me, unsigned int key, bool trylock) {
  MQLNode *qnode = &MQLNodeTable[me][key];
  qnode->init(LockMode::Reader, (uint32_t)SentinelValue::None, false, false,
              LockMode::None, LockStatus::Waiting,
              (uint32_t)SentinelValue::None);

  uint32_t p = tail.exchange(me);
  if (p == (uint32_t)SentinelValue::None) {
    nreaders++;
    qnode->granted.store(true, std::memory_order_release);
    return finish_acquire_reader_lock(me, key);
  }

  MQLNode *pred = &MQLNodeTable[p][key];
  // haven't set pred.next.id yet, safe to dereference pred
  if (pred->type.load(std::memory_order_acquire) == LockMode::Reader)
    return acquire_reader_lock_check_reader_pred(me, key, p, trylock);
  return acquire_reader_lock_check_writer_pred(me, key, p, trylock);
}

MQL_RESULT
MQLock::finish_acquire_reader_lock(uint32_t me, unsigned int key) {
  MQLNode *qnode = &MQLNodeTable[me][key];
  qnode->sucInfo.atomicStoreBusy(true);
  qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);

  // spin until me.next is not SuccessorLeaving
  while (qnode->sucInfo.atomicLoadNext() ==
         (uint32_t)SentinelValue::SuccessorLeaving)
    ;

  // if the lock tail now still points to me, truly no one is there, we're done
  if (tail.load(std::memory_order_acquire) == me) {
    // no successor if tail still points to me
    qnode->sucInfo.atomicStoreBusy(false);
    return MQL_RESULT::Acquired;
  }

  // note that the successor can't cancel now, ie me.next pointer is stable
  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None)
    ;

  uint32_t sucnum = qnode->sucInfo.atomicLoadNext();
  MQLNode *suc = &MQLNodeTable[sucnum][key];
  if (sucnum == (uint32_t)SentinelValue::None ||
      suc->type.load(std::memory_order_acquire) == LockMode::Writer) {
    qnode->sucInfo.atomicStoreBusy(false);
    return MQL_RESULT::Acquired;
  }

  // successor might be cancelling, in which case it'd xchg me.next.id to None
  // it's also possible that my cancelling writer successor is about to give me
  // a new reader successor, in this case my cancelling successor will realize
  // that I already have the lock and try to wake up the new successor directly
  // also by trying to change me.next.id to None (the new successor might spin
  // forever if its timeout is Never and the cancelling successor didn't wake it
  // up).
  //
  // if not CAS(me.next, successor, None)
  // tanabe. ここに来るということは，successor は何かしらいて，Reader である
  if (!qnode->sucInfo.atomicCASNext(sucnum, (uint32_t)SentinelValue::None)) {
    // tanabe. CAS に失敗したら，以前認識した successor
    // はキャンセルして離脱した．
    qnode->sucInfo.atomicStoreBusy(false);
    return MQL_RESULT::Acquired;
  }

  // if me.status is Granted and me.stype is None:
  // successor might have seen me in leaving state, it'll wait for me in that
  // case in this case, the successor saw me in leaving state and didn't
  // register as a reader. ie successor was acquiring
  while (suc->prev.load(std::memory_order_acquire) != me)
    ;
  if (suc->prev.compare_exchange_strong(me, (uint32_t)SentinelValue::Acquired,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
    nreaders++;
    suc->granted.store(true, std::memory_order_release);
    // make sure I know when releasing no need to wait
    qnode->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
  } else if (qnode->sucInfo.atomicLoadStype() == LockMode::Reader) {
    for (;;) {
      while (suc->prev.load(std::memory_order_acquire) == me)
        ;
      if (suc->prev.compare_exchange_strong(
              me, (uint32_t)SentinelValue::Acquired, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        nreaders++;
        suc->granted.store(true, std::memory_order_release);
        qnode->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
        break;
      }
    }
  }

  qnode->sucInfo.atomicStoreBusy(false);
  return MQL_RESULT::Acquired;
}

MQL_RESULT
MQLock::acquire_reader_lock_check_reader_pred(uint32_t me, unsigned int key,
                                              uint32_t pred, bool trylock) {
check_pred:
  uint32_t pretail;
  MQLNode *qnode = &MQLNodeTable[me][key];
  MQLNode *p = &MQLNodeTable[pred][key];
  // wait for the previous canceling dude to leave
  while (!(p->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None &&
           p->sucInfo.atomicLoadStype() == LockMode::None))
    ;

  MQLMetaInfo expected, desired;
  expected.init(false, LockMode::None, LockStatus::Waiting,
                (uint32_t)SentinelValue::None);
  desired.init(false, LockMode::Reader, LockStatus::Waiting, me);
  __atomic_compare_exchange_n(&(p->sucInfo.obj), &expected.obj, desired.obj,
                              false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);

  if (p->sucInfo.atomicLoadBusy() == false &&
      p->sucInfo.atomicLoadStype() == LockMode::Reader &&
      p->sucInfo.atomicLoadStatus() == LockStatus::Waiting) {  // succeeded
    // link_pred(pred, me)
    p->sucInfo.atomicStoreNext(me);

    // if me.granted becomes True before timing out
    //    return finish_reader_acquire
    // return cancel_reader_lock
    if (trylock) {
      if (qnode->granted.load(std::memory_order_acquire))
        return finish_acquire_reader_lock(me, key);
      else
        return cancel_reader_lock(me, key);
    } else {
      // tanabe. trylock でないなら，無限に待つべき．
      while (qnode->granted.load(std::memory_order_acquire) != true)
        ;
      return finish_acquire_reader_lock(me, key);
    }
  }

  // Failed cases
  if (p->sucInfo.atomicLoadStatus() == LockStatus::Leaving) {
    // don't set pred.next.successor_class here
    p->sucInfo.atomicStoreNext(me);

    // if pred did cancel, it will give me a new pred;
    // if it got the lock it will wake me up
    qnode->prev.store(pred, std::memory_order_release);
    while (qnode->prev.load(std::memory_order_acquire) != pred)
      ;
    // consume it and retry
    pretail = qnode->prev.exchange((uint32_t)SentinelValue::None);
    if (pretail == (uint32_t)SentinelValue::Acquired) {
      while (qnode->granted.load(std::memory_order_acquire) != true)
        ;
      return finish_acquire_reader_lock(me, key);
    }
    p = &MQLNodeTable[pretail][key];
    if (p->type.load(std::memory_order_acquire) == LockMode::Writer)
      return acquire_reader_lock_check_writer_pred(me, key, pretail, trylock);
    pred = pretail;
    goto check_pred;  // p must point to a valid predecessor;
  } else {
    // pred is granted - might be a direct grant or grant in the leaving process
    // I didn't register, pred won't wake me up, but if pred is leaving_granted,
    // we need to tell it not to poke me in its finish-acquire call.
    // For direct_granted,
    // also set its next.id to None so it knows that there's no need to wait and
    // examine successor upon release. This also covers the
    // case when pred.next.flags has Busy set.
    p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
    nreaders++;
    qnode->granted.store(true, std::memory_order_release);
    return finish_acquire_reader_lock(me, key);
  }
}

MQL_RESULT
MQLock::cancel_reader_lock(uint32_t me, unsigned int key) {
  NNN;
  MQLNode *qnode = &MQLNodeTable[me][key];
  uint32_t pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
  // prevent from cancelling

  if (pred == (uint32_t)SentinelValue::Acquired) {
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    return finish_acquire_reader_lock(me, key);
  }

  // make sure successor can't leave, unless it tried to leave first
  qnode->sucInfo.atomicStoreStatus(LockStatus::Leaving);
  while (qnode->sucInfo.atomicLoadNext() ==
         (uint32_t)SentinelValue::SuccessorLeaving)
    ;

  // pred not equal qnode->prev.
  // この関数冒頭で exchange しているから．
  if (MQLNodeTable[pred][key].type.load(std::memory_order_acquire) ==
      LockMode::Reader) {
    return cancel_reader_lock_with_reader_pred(me, key, pred);
  }
  return cancel_reader_lock_with_writer_pred(me, key, pred);
}

MQL_RESULT
MQLock::cancel_reader_lock_with_writer_pred(uint32_t me, unsigned int key,
                                            uint32_t pred) {
  NNN;
retry:
  MQLNode *qnode = &MQLNodeTable[me][key];
  MQLNode *p = &MQLNodeTable[pred][key];
  // wait for the cancelling pred to finish relink
  // spin until pred.next is me and pred.stype is Reader
  // pred is a writer, so I can go as long as it's not also leaving (cancelling
  // or releasing)
  for (;;) {
    // must take a copy first
    MQLMetaInfo eflags = p->atomicLoadSucInfo();
    if (eflags.status == LockStatus::Leaving) {
      // must wait for pred to give me a new pred (or wait to be waken up?)
      // pred should give me a new pred, after its CAS trying to pass me the
      // lock failed
      qnode->prev.store(pred, std::memory_order_release);
      while (qnode->prev.load(std::memory_order_acquire) != pred)
        ;
      pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
      if (pred == (uint32_t)SentinelValue::None ||
          pred == (uint32_t)SentinelValue::Acquired) {
        while (qnode->granted.load(std::memory_order_acquire) != true)
          ;
        return finish_acquire_reader_lock(me, key);
      } else {
        // make sure successor can't leave, unless it tried to leave first
        qnode->sucInfo.atomicStoreStatus(LockStatus::Leaving);
        while (qnode->sucInfo.atomicLoadNext() ==
               (uint32_t)SentinelValue::SuccessorLeaving)
          ;
        // (tanabe) pred may be changed to new value at L:340
        p = &MQLNodeTable[pred][key];
        if (p->type.load(std::memory_order_acquire) == LockMode::Reader)
          // (tanabe) if it gets new reader pred, it executes cancel for reader
          return cancel_reader_lock_with_reader_pred(me, key, pred);
        // (tanabe) get new pred because old pred left.
        goto retry;
      }
    } else if (eflags.busy == true) {
      qnode->prev.store(pred, std::memory_order_release);
      while (qnode->granted.load(std::memory_order_acquire) != true)
        ;
      return finish_acquire_reader_lock(me, key);
    }
    // try to tell pred I'm leaving
    p = &MQLNodeTable[pred][key];
    MQLMetaInfo expected, desired;
    expected = eflags;
    expected.next = me;
    desired = expected;
    desired.next = (uint32_t)SentinelValue::SuccessorLeaving;
    if (p->atomicCASSucInfo(expected, desired)) break;
  }

  // pred now has SuccessorLeaving on its next.id, it won't try to wake me up
  // during release now link the new successor and pred
  if (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None &&
      tail.compare_exchange_strong(me, pred, std::memory_order_acq_rel,
                                   std::memory_order_acquire)) {
    p = &MQLNodeTable[pred][key];
    p->sucInfo.atomicStoreStype(LockMode::None);
    p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
    return MQL_RESULT::Cancelled;
  }

  cancel_reader_lock_relink(pred, me, key);
  return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_reader_lock_with_reader_pred(uint32_t me, unsigned int key,
                                            uint32_t pred) {
  NNN;
retry:
  MQLNode *qnode = &MQLNodeTable[me][key];
  MQLNode *p = &MQLNodeTable[pred][key];
  // now successor can't attach to me assuming I'm waiting or has already done
  // so. CAS out of pred.next (including id and flags) wait for the canceling
  // pred to finish the relink spin until pred.stype is Reader and (pred.next is
  // me or pred.next is NoSuccessor only want to put SuccessorLeaving in the id
  // field
  MQLMetaInfo expected, desired;
  expected.init(false, LockMode::Reader, LockStatus::Waiting, me);
  desired.init(false, LockMode::Reader, LockStatus::Waiting,
               (uint32_t)SentinelValue::SuccessorLeaving);
  if (!p->atomicCASSucInfo(expected, desired)) {
    // Note: we once registered after pred as a reader successor (still are), so
    // if pred happens to get the lock, it will wake me up seeing its
    // reader_successor set
    if (p->sucInfo.atomicLoadStatus() == LockStatus::Granted) {
      // pred will in its finish-acquire-reader() wake me up.
      // pred already should alredy have me on its next.id and has reader
      // successor class, (the CAS loop in the "acquire" block). this also
      // covers the case when pred.next.flags has busy set.
      qnode->prev.store(pred, std::memory_order_release);
      while (qnode->granted.load(std::memory_order_acquire) != true)
        ;
      return finish_acquire_reader_lock(me, key);
    } else {
      // pred is trying to leave, wait for a new pred or being waken up
      // pred has higher priority to leave, and it should already have me on its
      // next.id
      qnode->prev.store(pred, std::memory_order_release);
      while (qnode->prev.load(std::memory_order_acquire) != pred)
        ;
      // consume it and retry
      pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
      if (pred == (uint32_t)SentinelValue::Acquired) {
        while (qnode->granted.load(std::memory_order_acquire) != true)
          ;
        return finish_acquire_reader_lock(me, key);
      }
      MQLNode *p = &MQLNodeTable[pred][key];
      if (p->type.load(std::memory_order_acquire) == LockMode::Writer)
        return cancel_reader_lock_with_writer_pred(me, key, pred);
      goto retry;
    }
  } else {
    // at this point pred will be waiting for a new successor if it decides
    // to move and successor will be waiting for a new pred
    if (qnode->sucInfo.atomicLoadStype() == LockMode::None &&
        tail.compare_exchange_strong(me, pred, std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
      // newly arriving successor for this pred will wait
      // for the SuccessorLeaving mark to go away before trying the CAS
      p->sucInfo.atomicStoreStype(LockMode::None);
      p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);
      return MQL_RESULT::Cancelled;
    }
    cancel_reader_lock_relink(pred, me, key);
  }
  return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_reader_lock_relink(uint32_t pred, uint32_t me,
                                  unsigned int key) {
  NNN;
  MQLNode *qnode = &MQLNodeTable[me][key];
  MQLNode *p = &MQLNodeTable[pred][key];
  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None)
    ;
  for (;;) {  // preserve pred.flags
    MQLMetaInfo expected, desired;
    expected = p->atomicLoadSucInfo();
    desired = expected;
    desired.next = qnode->sucInfo.atomicLoadNext();
    desired.stype = qnode->sucInfo.atomicLoadStype();
    desired.busy = qnode->sucInfo.atomicLoadBusy();
    if (p->atomicCASSucInfo(expected, desired)) break;
  }

  // I believe we should do this after setting pred.id, see the comment in
  // cancel_writer_lock. retry untill CAS(me,next.prev, me, pred) is True
  for (;;) {
    MQLNode *suc = &MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key];
    uint32_t expected = me;
    if (suc->prev.compare_exchange_strong(expected, pred,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire))
      break;
  }
  return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::acquire_reader_lock_check_writer_pred(uint32_t me, unsigned int key,
                                              uint32_t pred, bool trylock) {
  // wait for the previous canceling dude to leave spin
  // until pred.next is NULL and pred.stype is None
  // pred is a writer, we have to wait anyway, so register and wait with timeout
  MQLNode *qnode = &MQLNodeTable[me][key];
  MQLNode *p = &MQLNodeTable[pred][key];
  p->sucInfo.atomicStoreStype(LockMode::Reader);
  p->sucInfo.atomicStoreNext(me);
  if (qnode->prev.exchange(pred) == (uint32_t)SentinelValue::Acquired) {
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    return finish_acquire_reader_lock(me, key);
  }

  if (trylock) {
    if (qnode->granted.load(std::memory_order_acquire))
      return finish_acquire_reader_lock(me, key);
    else
      return cancel_reader_lock(me, key);
  } else {
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    return finish_acquire_reader_lock(me, key);
  }
}

void MQLock::release_reader_lock(uint32_t me, unsigned int key) {
  // make sure successor can't leave; readers, however, can still get the lock
  // as usual by seeing me.next.flags.granted set
  MQLNode *qnode = &MQLNodeTable[me][key];
  qnode->sucInfo.atomicStoreBusy(true);
  while (qnode->sucInfo.atomicLoadNext() ==
         (uint32_t)SentinelValue::SuccessorLeaving)
    ;

  uint32_t expected;
  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None) {
    expected = me;
    if (tail.compare_exchange_strong(expected, (uint32_t)SentinelValue::None,
                                     std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
      return finish_release_reader_lock(me, key);
    }
    printf("th %d : rl rl %d : next %d tail %d\n", me, key,
           qnode->sucInfo.atomicLoadNext(), tail.load());
  }

  if (qnode->sucInfo.atomicLoadNext() != (uint32_t)SentinelValue::None &&
      qnode->sucInfo.atomicLoadStype() == LockMode::Writer) {
    // put it in next_writer
    next_writer = (uint32_t)qnode->sucInfo.atomicLoadNext();
    // also tell successor it doesn't have pred any more
    MQLNode *suc = &MQLNodeTable[next_writer][key];
    expected = me;
    while (!suc->prev.compare_exchange_strong(
        expected, (uint32_t)SentinelValue::None, std::memory_order_acq_rel,
        std::memory_order_acquire)) {
      expected = me;
    }
  }

  return finish_release_reader_lock(me, key);
}

void MQLock::finish_release_reader_lock(uint32_t me, unsigned int key) {
  if (nreaders.fetch_sub(1) == 1) {
    // I'm the last reader, must handle the next writer.
    uint32_t nw = next_writer;
    if (nw != (uint32_t)SentinelValue::None &&
        nreaders.load(std::memory_order_acquire) == 0 &&
        next_writer.compare_exchange_strong(nw, (uint32_t)SentinelValue::None,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
      for (;;) {
        uint32_t expected = (uint32_t)SentinelValue::None;
        if (MQLNodeTable[nw][key].prev.compare_exchange_strong(
                expected, (uint32_t)SentinelValue::Acquired,
                std::memory_order_acq_rel, std::memory_order_acquire))
          break;
      }
      MQLNodeTable[nw][key].granted.store(true, std::memory_order_release);
    }
  }
}

MQL_RESULT
MQLock::acquire_writer_lock(uint32_t me, unsigned int key, bool trylock) {
  MQLNode *qnode = &MQLNodeTable[me][key];
  qnode->init(LockMode::Writer, (uint32_t)SentinelValue::None, false, false,
              LockMode::None, LockStatus::Waiting,
              (uint32_t)SentinelValue::None);

  uint32_t pred = tail.exchange(me);
  MQLNode *p = &MQLNodeTable[pred][key];
  if (pred == (uint32_t)SentinelValue::None) {
    next_writer.store(me, std::memory_order_release);
    if (nreaders.load(std::memory_order_acquire) == 0 &&
        next_writer.exchange((uint32_t)SentinelValue::None) == me) {
      qnode->granted.store(true, std::memory_order_release);
      return MQL_RESULT::Acquired;
    }
  } else {
    // MQLNode *p = &MQLNodeTable[pred][key];
    // spin until pred.stype is None and pred.next is NULL
    while (!(p->sucInfo.atomicLoadStype() == LockMode::None &&
             p->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None)) {
      printf("th %d : aq wl %d : p->stype %d p->next %d\n", me, key,
             (int)p->sucInfo.atomicLoadStype(), p->sucInfo.atomicLoadNext());
    }
    // printf("th %d : acquire wl %d\n", me, key);
    // register on pred.flags as a writer successor,
    // then fill in pred.next.id and wait
    // must register on pred.flags first
    // p->sucInfo.atomicStoreStype(LockMode::Writer);
    // p->sucInfo.atomicStoreNext(me);
  }
  p->sucInfo.atomicStoreStype(LockMode::Writer);
  p->sucInfo.atomicStoreNext(me);

  if (qnode->prev.exchange(pred) == (uint32_t)SentinelValue::Acquired) {
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
    return MQL_RESULT::Acquired;
  }

  if (trylock) {
    if (qnode->granted.load(std::memory_order_acquire)) {
      qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
      return MQL_RESULT::Acquired;
    } else {
      return cancel_writer_lock(me, key);
    }
  }

  while (qnode->granted.load(std::memory_order_acquire) != true) {
    printf("th %d : aq wl %d : infinite roop, pred %d\n", me, key, pred);
  }
  qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
  return MQL_RESULT::Acquired;
}

void MQLock::release_writer_lock(uint32_t me, unsigned int key) {
  MQLNode *qnode = &MQLNodeTable[me][key];
  qnode->sucInfo.atomicStoreBusy(true);
  // make sure successor can't leave
  while (qnode->sucInfo.atomicLoadNext() ==
         (uint32_t)SentinelValue::SuccessorLeaving)
    ;

  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None) {
    // printf("th %d : release wl %d\n", me, key);
    uint32_t expected, desired;
    expected = me;
    desired = (uint32_t)SentinelValue::None;
    if (tail.compare_exchange_strong(expected, desired,
                                     std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
      qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
                  LockMode::None, LockStatus::Waiting,
                  (uint32_t)SentinelValue::None);
      return;
    }
    printf("th %d : rl wl %d : next %d tail %d\n", me, key,
           qnode->sucInfo.atomicLoadNext(), tail.load());
  }

  MQLNode *suc;
  for (;;) {
    uint32_t expected(me), desired((uint32_t)SentinelValue::Acquired);
    suc = &MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key];
    if (suc->prev.compare_exchange_strong(expected, desired,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire))
      break;
  }

  if (suc->type.load(std::memory_order_acquire) == LockMode::Reader) nreaders++;
  suc->granted.store(true, std::memory_order_release);

  qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
              LockMode::None, LockStatus::Waiting,
              (uint32_t)SentinelValue::None);
  return;
}

MQL_RESULT
MQLock::cancel_writer_lock(uint32_t me, unsigned int key) {
  NNN;
start_cancel:
  MQLNode *qnode = &MQLNodeTable[me][key];
  uint32_t pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
  MQLNode *p = &MQLNodeTable[pred][key];
  // if pred is a releasing writer and already dereference my id, it will CAS
  // me.pred.id to Acquired, so we do a final check here; there's no way back
  // after this point (unless pred is a reader and it's already gone). After my
  // xchg, pred will be waiting for me to give it a new successor.
  if (pred == (uint32_t)SentinelValue::Acquired) {
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
    return MQL_RESULT::Acquired;
  }

  // "freeze" the successor
  qnode->sucInfo.atomicStoreStatus(LockStatus::Leaving);
  while (qnode->sucInfo.atomicLoadNext() ==
         (uint32_t)SentinelValue::SuccessorLeaving)
    ;

  // if I still have a pred, then deregister from it;
  // if I don't have a pred,
  // that means my pred has put me on next_writer, deregister from there and go.
  // Note that the reader should first reset me.pred.id,
  // then put me on lock.nw
  if (qnode->prev.load(std::memory_order_acquire) ==
      (uint32_t)SentinelValue::None)
    return cancel_writer_lock_no_pred(me, key);

  for (;;) {
    // wait for cancelling pred to finish relink, note pred_block is updated
    // later in the if block as well
    while (!(p->sucInfo.atomicLoadNext() == me &&
             p->sucInfo.atomicLoadStype() == LockMode::Writer))
      ;

    // whatever flags value it might have, just not Leaving
    MQLMetaInfo pflags = p->atomicLoadSucInfo();
    if (pflags.status == LockStatus::Leaving) {
      // pred might be cancelling, we won't know if it'll eventually
      // get the lock or really cancel.
      // In the former case it won't update my pred;
      // in the later case it will.
      // So just recover me.pred.id and retry (can't reset
      // next.flags to Waiting - that will confuse our successor).
      qnode->prev.store(pred, std::memory_order_release);
      goto start_cancel;
    } else if (pflags.busy == true) {
      // pred is perhaps releasing (writer)? me.pred.id is 0,
      // pred can do nothing about me,
      // so it's safe to dereference
      if (p->type == LockMode::Writer) {
        qnode->prev.store(pred, std::memory_order_release);
        while (qnode->granted.load(std::memory_order_acquire) != true)
          ;
        qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
        return MQL_RESULT::Acquired;
      }
      qnode->prev.store(pred, std::memory_order_release);
      pred = qnode->prev.exchange((uint32_t)SentinelValue::None);
      if (pred == (uint32_t)SentinelValue::None)
        return cancel_writer_lock_no_pred(me, key);
      else if (pred == (uint32_t)SentinelValue::Acquired) {
        while (qnode->granted.load(std::memory_order_acquire) != true)
          ;
        qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
        return MQL_RESULT::Acquired;
      }
      continue;  // retry if it's a reader
    }

    MQLMetaInfo expected, desired;
    desired = pflags;
    desired.next = (uint32_t)SentinelValue::SuccessorLeaving;
    expected = pflags;
    expected.next = me;
    if (p->atomicCASSucInfo(expected, desired)) break;
  }

  {
    // This scope means taht I want to use the name "expected", "desired" later.
    uint32_t expected(me), desired(pred);
    if (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None &&
        tail.compare_exchange_strong(expected, desired,
                                     std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
      p->sucInfo.atomicStoreStype(LockMode::None);
      p->sucInfo.atomicStoreNext((uint32_t)SentinelValue::None);

      // initialize
      MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key].prev.store(
          (uint32_t)SentinelValue::None, std::memory_order_release);
      qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
                  LockMode::None, LockStatus::Waiting,
                  (uint32_t)SentinelValue::None);
      return MQL_RESULT::Cancelled;
    }
  }

  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None)
    ;
  MQLMetaInfo successor;
  successor.init(false, qnode->sucInfo.atomicLoadStype(), LockStatus::Waiting,
                 qnode->sucInfo.atomicLoadNext());

retry:
  // preserve pred.flags
  MQLMetaInfo expected;
  expected = p->atomicLoadSucInfo();
  bool wakeup = false;

  if (p->type.load(std::memory_order_acquire) == LockMode::Reader &&
      MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key].type.load(
          std::memory_order_acquire) == LockMode::Reader &&
      p->sucInfo.atomicLoadStatus() == LockStatus::Granted) {
    // There is a time window which starts after the pred finishedits "acquired"
    // block and ends before it releases. During this period my relink is
    // essentially invisible to pred. So we try to wake up the successor if this
    // the case.
    successor.next = (uint32_t)SentinelValue::None;
    wakeup = true;
  }

  if (p->sucInfo.atomicLoadBusy() == true) {
    successor.busy = true;
    if (!p->atomicCASSucInfo(expected, successor)) goto retry;
  }

  // Now we need to wake up the successor if needed and set suc.pred.id - must
  // set suc.pred.id after setting pred.next.id: if we need to wake up
  // successor, we need to also set pred.next.id to NoSuccessor, which makes it
  // not safe for succ to spin on pred.next.id to wait for me finishing this
  // relink (pred might disappear any time because its next.id is NoSuccessor).
  MQLNode *suc = &MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key];
  if (wakeup) {
    nreaders++;
    suc->granted.store(true, std::memory_order_release);
    for (;;) {
      uint32_t localme = me;
      if (suc->prev.compare_exchange_strong(
              localme, (uint32_t)SentinelValue::Acquired,
              std::memory_order_acq_rel, std::memory_order_acquire))
        break;
    }
  } else {
    for (;;) {
      uint32_t localme = me;
      if (suc->prev.compare_exchange_strong(localme, pred,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire))
        break;
    }
  }

  MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key].prev.store(
      (uint32_t)SentinelValue::None, std::memory_order_release);
  qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
              LockMode::None, LockStatus::Waiting,
              (uint32_t)SentinelValue::None);
  return MQL_RESULT::Cancelled;
}

MQL_RESULT
MQLock::cancel_writer_lock_no_pred(uint32_t me, unsigned int key) {
  NNN;
  MQLNode *qnode = &MQLNodeTable[me][key];
  while (!(next_writer != (uint32_t)SentinelValue::None ||
           qnode->granted.load(std::memory_order_acquire) == true))
    ;
  uint32_t localme(me);
  if (qnode->granted.load(std::memory_order_acquire) == true ||
      !next_writer.compare_exchange_strong(
          localme, (uint32_t)SentinelValue::None, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    // reader picked me up...
    while (qnode->granted.load(std::memory_order_acquire) != true)
      ;
    qnode->sucInfo.atomicStoreStatus(LockStatus::Granted);
    return MQL_RESULT::Acquired;
  }

  // so lock.next_writer is null now, try to fix the lock tail
  localme = me;
  if (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None &&
      tail.compare_exchange_strong(localme, (uint32_t)SentinelValue::None,
                                   std::memory_order_acq_rel,
                                   std::memory_order_acquire)) {
    MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key].prev.store(
        (uint32_t)SentinelValue::None, std::memory_order_release);
    qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
                LockMode::None, LockStatus::Waiting,
                (uint32_t)SentinelValue::None);
    return MQL_RESULT::Cancelled;
  }

  while (qnode->sucInfo.atomicLoadNext() == (uint32_t)SentinelValue::None)
    ;
  uint32_t localnext = qnode->sucInfo.atomicLoadNext();
  // must copy first;
  //
  // because I don't have a pred, if next_id is a writer, I should put it in
  // lock.nw
  MQLNode *suc = &MQLNodeTable[localnext][key];
  if (suc->type.load(std::memory_order_acquire) == LockMode::Writer) {
    // remaining readers will use CAS on lock.nw, so we blind write
    next_writer.store(localnext, std::memory_order_release);
    for (;;) {
      uint32_t forlocalme(me);
      if (suc->prev.compare_exchange_strong(
              forlocalme, (uint32_t)SentinelValue::None,
              std::memory_order_acq_rel, std::memory_order_acquire))
        break;
    }

    uint32_t casnext(localnext);
    if (nreaders.load(std::memory_order_acquire) == 0 &&
        next_writer.compare_exchange_strong(
            casnext, (uint32_t)SentinelValue::None, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      // ok, I'm so nice, cancelled myself and woke up a successor
      for (;;) {
        uint32_t forSentiNone = (uint32_t)SentinelValue::None;
        if (suc->prev.compare_exchange_strong(
                forSentiNone, (uint32_t)SentinelValue::Acquired,
                std::memory_order_acq_rel, std::memory_order_acquire))
          ;
      }
      suc->granted.store(true, std::memory_order_release);
    }
  } else {
    // successor is a reader, lucky for it ...
    for (;;) {
      uint32_t forme(me);
      if (suc->prev.compare_exchange_strong(
              forme, (uint32_t)SentinelValue::Acquired,
              std::memory_order_acq_rel, std::memory_order_acquire))
        break;
    }
    nreaders++;
    suc->granted.store(true, std::memory_order_release);
  }

  MQLNodeTable[qnode->sucInfo.atomicLoadNext()][key].prev.store(
      (uint32_t)SentinelValue::None, std::memory_order_release);
  qnode->init(LockMode::None, (uint32_t)SentinelValue::None, false, false,
              LockMode::None, LockStatus::Waiting,
              (uint32_t)SentinelValue::None);
  return MQL_RESULT::Cancelled;
}
#endif  // MQLOCK

#ifdef RWLOCK
void RWLock::r_lock() {
  int expected, desired;
  expected = counter_.load(std::memory_order_acquire);
  for (;;) {
    if (expected != -1)
      desired = expected + 1;
    else {
      expected = counter_.load(std::memory_order_acquire);
      continue;
    }
    if (counter_.compare_exchange_strong(expected, desired,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire))
      break;
  }
}

bool RWLock::r_trylock() {
  int expected, desired;
  expected = counter_.load(std::memory_order_acquire);
  for (;;) {
    if (expected != -1)
      desired = expected + 1;
    else
      return false;

    if (counter_.compare_exchange_strong(expected, desired,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire))
      return true;
  }
}

void RWLock::r_unlock() { counter_--; }

void RWLock::w_lock() {
  int expected, desired(-1);
  expected = counter_.load(std::memory_order_acquire);
  for (;;) {
    if (expected != 0) {
      expected = counter_.load(std::memory_order_acquire);
      continue;
    }
    if (counter_.compare_exchange_strong(expected, desired,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire))
      break;
  }
}

bool RWLock::w_trylock() {
  int expected, desired(-1);
  expected = counter_.load(std::memory_order_acquire);
  for (;;) {
    if (expected != 0) return false;
    if (counter_.compare_exchange_strong(expected, desired,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire))
      return true;
  }
}

void RWLock::w_unlock() { counter_++; }

bool RWLock::upgrade() {
  int expected = 1;
  return counter_.compare_exchange_weak(expected, -1,
                                        std::memory_order_acq_rel);
}
#endif  // RWLOCK
