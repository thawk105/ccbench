#include "frame.hh"

using namespace std;

extern std::atomic<uint64_t> timestampcounter;
extern std::mutex SsnLock;

void print_mode()
{
    cout << "this result is executed by RC+SSN" << endl;
}

void Transaction::tbegin()
{
    this->txid_ = atomic_fetch_add(&timestampcounter, 1);
    ssn_tbegin();
}

void Transaction::tread(uint64_t key)
{
    // read-own-writes, re-read from previous read in the same tx.
    if (searchWriteSet(key) == true || searchReadSet(key) == true)
        goto FINISH_TREAD;

    // get version to read
    Tuple *tuple;
    tuple = get_tuple(key);
    Version *ver;
    ver = tuple->latest_.load(memory_order_acquire);
    while (ver->status_.load(memory_order_acquire) != Status::committed || txid_ < ver->cstamp_.load(memory_order_acquire))
    {
        ver = ver->prev_;
        ++res_->local_traversal_counts_;
    }

    ssn_tread(ver, key);

    if (this->status_ == Status::aborted)
    {
        ++res_->local_readphase_counts_;
        goto FINISH_TREAD;
    }
FINISH_TREAD:
    return;
}

void Transaction::twrite(uint64_t key, uint64_t write_val)
{
    // update local write set
    if (searchWriteSet(key) == true)
        return;

    Tuple *tuple;
    tuple = get_tuple(key);

    // If v not in t.writes:
    Version *expected, *desired;
    desired = new Version();
    desired->cstamp_.store(this->txid_, memory_order_release);
    desired->val_ = write_val;

    expected = tuple->latest_.load(memory_order_acquire);
    for (;;)
    {
        // prevent dirty write
        if (expected->status_.load(memory_order_acquire) == Status::inFlight)
        {
            if (this->txid_ <= expected->cstamp_.load(memory_order_acquire))
            {
                this->status_ = Status::aborted;
                ++res_->local_wwconflict_counts_;
                goto FINISH_TWRITE;
            }
            expected = tuple->latest_.load(memory_order_acquire);
            continue;
        }
        else
            break;
    }

    expected->mt_.lock();
    desired->prev_ = expected;
    tuple->latest_ = desired;
    expected->mt_.unlock();

    ssn_twrite(desired, key);

    if (this->status_ == Status::aborted)
        ++res_->local_writephase_counts_;

FINISH_TWRITE:
    return;
}

void Transaction::commit()
{
    this->cstamp_ = atomic_fetch_add(&timestampcounter, 1);

    // begin pre-commit
    SsnLock.lock();

    ssn_commit();

    if (this->status_ == Status::committed)
    {
        for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
        {
            (*itr).ver_->cstamp_.store(this->cstamp_, memory_order_release);
            (*itr).ver_->status_.store(Status::committed, memory_order_release);
        }
        SsnLock.unlock();
    }
    else
    {
        if (this->status_ == Status::inFlight)
            cout << "commit error" << endl;
        SsnLock.unlock();
        return;
    }
    read_set_.clear();
    write_set_.clear();

    if (this->abortcount_ != 0)
        res_->local_additionalabort.push_back(this->abortcount_);
    this->abortcount_ = 0;
    return;
}

void Transaction::abort()
{
    ssn_abort();

    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
    {
        (*itr).ver_->status_.store(Status::aborted, memory_order_release);
    }
    write_set_.clear();

    // notify that this transaction finishes reading the version now.
    read_set_.clear();
    ++res_->local_abort_counts_;
    if (isreadonly())
    {
        ++res_->local_readonly_abort_counts_;
        ++this->abortcount_;
    }
}