#include "frame.hh"

using namespace std;

void Result::displayAllResult()
{
    cout << "abort_counts_:\t\t\t" << total_abort_counts_ << endl;
    cout << "commit_counts_:\t\t\t" << total_commit_counts_ << endl;
    cout << "read SSNcheck abort:\t\t" << total_readphase_counts_ << endl;
    cout << "write SSNcheck abort:\t\t" << total_writephase_counts_ << endl;
    cout << "commit SSNcheck abort:\t\t" << total_commitphase_counts_ << endl;
    cout << "ww conflict abort:\t\t" << total_wwconflict_counts_ << endl;
    cout << "total_readonly_abort:\t\t" << total_readonly_abort_counts_ << endl;
    cout << "total_read deadlock_abort:\t" << total_rdeadlock_abort_counts_ << endl;
    cout << "total_write deadlock_abort:\t" << total_wdeadlock_abort_counts_ << endl;
    // displayAbortRate
    long double ave_rate =
        (double)total_abort_counts_ /
        (double)(total_commit_counts_ + total_abort_counts_);
    cout << fixed << setprecision(4) << "abort_rate:\t\t\t" << ave_rate << endl;
    // cout << "traversal counts:\t\t" << total_traversal_counts_ << endl;

    // count the number of recursing abort
    vector<int> tmp;
    tmp = total_additionalabort;
    std::sort(tmp.begin(), tmp.end());
    tmp.erase(std::unique(tmp.begin(), tmp.end()), tmp.end());
    for (auto itr = tmp.begin(); itr != tmp.end(); itr++)
    {
        size_t count = std::count(total_additionalabort.begin(), total_additionalabort.end(), *itr);
        cout << *itr << " " << count << endl;
    }
}

void Result::addLocalAllResult(const Result &other)
{
    total_abort_counts_ += other.local_abort_counts_;
    total_commit_counts_ += other.local_commit_counts_;
    total_readphase_counts_ += other.local_readphase_counts_;
    total_writephase_counts_ += other.local_writephase_counts_;
    total_commitphase_counts_ += other.local_commitphase_counts_;
    total_wwconflict_counts_ += other.local_wwconflict_counts_;
    total_traversal_counts_ += other.local_traversal_counts_;
    total_readonly_abort_counts_ += other.local_readonly_abort_counts_;
    total_additionalabort.insert(total_additionalabort.end(), other.local_additionalabort.begin(), other.local_additionalabort.end());
    total_rdeadlock_abort_counts_ += other.local_rdeadlock_abort_counts_;
    total_wdeadlock_abort_counts_ += other.local_wdeadlock_abort_counts_;
}

bool isReady(const std::vector<char> &readys)
{
    for (const char &b : readys)
    {
        if (!b)
            return false;
    }
    return true;
}

void waitForReady(const std::vector<char> &readys)
{
    while (!isReady(readys))
    {
        _mm_pause();
    }
}

std::vector<Result> ErmiaResult;

void initResult() { ErmiaResult.resize(thread_num); }

std::atomic<uint64_t> timestampcounter(1); // timestampを割り当てる

void Version::init()
{
    pstamp_.store(0, std::memory_order_release);
    sstamp_.store(UINT32_MAX, std::memory_order_release);
    cstamp_.store(0, std::memory_order_release);
    status_.store(Status::inFlight, std::memory_order_release);
    pstamp_for_rlock_.store(0, std::memory_order_release);
    locked_flag_ = false;
    this->prev_ = nullptr;
}

Tuple *Table;       // databaseのtable
std::mutex SsnLock; // giant lock

void makeTask(std::vector<Task> &tasks, Xoroshiro128Plus &rnd, FastZipf &zipf)
{
    tasks.clear();
    if ((rnd.next() % 100) < ronly_ratio)
    {
        for (size_t i = 0; i < max_ope_readonly; ++i)
        {
            uint64_t tmpkey;
            // decide access destination key.
            tmpkey = zipf() % tuple_num;
            tasks.emplace_back(Ope::READ, tmpkey);
        }
    }
    else
    {
        for (size_t i = 0; i < max_ope; ++i)
        {
            uint64_t tmpkey;
            // decide access destination key.
            tmpkey = zipf() % tuple_num;
            // decide operation type.
            if ((rnd.next() % 100) < rratio)
            {
                tasks.emplace_back(Ope::READ, tmpkey);
            }
            else
            {
                tasks.emplace_back(Ope::WRITE, tmpkey, zipf());
            }
        }
    }
}

bool Transaction::searchReadSet(unsigned int key)
{
    for (auto itr = read_set_.begin(); itr != read_set_.end(); ++itr)
    {
        if ((*itr).key_ == key)
            return true;
    }
    return false;
}

bool Transaction::searchWriteSet(unsigned int key)
{
    for (auto itr = write_set_.begin(); itr != write_set_.end(); ++itr)
    {
        if ((*itr).key_ == key)
            return true;
    }
    return false;
}

Tuple *Transaction::get_tuple(uint64_t key)
{
    return (&Table[key]);
}

bool Transaction::isreadonly()
{
    for (auto itr = task_set_.begin(); itr != task_set_.end(); itr++)
    {
        if (itr->ope_ == Ope::WRITE)
        {
            return false;
        }
    }
    return true;
}

void displayParameter()
{
    cout << "max_ope:\t\t\t" << max_ope << endl;
    cout << "rratio:\t\t\t\t" << rratio << endl;
    cout << "thread_num:\t\t\t" << thread_num << endl;
}

void makeDB()
{
    posix_memalign((void **)&Table, PAGE_SIZE, (tuple_num) * sizeof(Tuple));
    for (int i = 0; i < tuple_num; i++)
    {
        Table[i].key = 0;
        Version *verTmp = new Version();
        verTmp->status_.store(Status::committed, memory_order_release);
        verTmp->val_ = 0;
        Table[i].latest_.store(verTmp, memory_order_release);
    }
}

void worker(size_t thid, char &ready, const bool &start, const bool &quit)
{
    Xoroshiro128Plus rnd;
    rnd.init();
    Result &myres = std::ref(ErmiaResult[thid]);
    FastZipf zipf(&rnd, skew, tuple_num);

    Transaction trans(thid, (Result *)&ErmiaResult[thid]);

    ready = true;

    while (start == false)
    {
    }

    while (quit == false)
    {
        makeTask(trans.task_set_, rnd, zipf);
        // viewtask(trans.task_set_);

    RETRY:
        if (quit == true)
            break;

        trans.tbegin();
        for (auto itr = trans.task_set_.begin(); itr != trans.task_set_.end();
             ++itr)
        {
            if ((*itr).ope_ == Ope::READ)
            {
                trans.tread((*itr).key_);
            }
            else if ((*itr).ope_ == Ope::WRITE)
            {
                trans.twrite((*itr).key_, (*itr).write_val_);
            }
            // early abort.
            if (trans.status_ == Status::aborted)
            {
                trans.abort();

                goto RETRY;
            }
        }
        trans.commit();
        if (trans.status_ == Status::committed)
        {
            myres.local_commit_counts_++;
        }
        else if (trans.status_ == Status::aborted)
        {
            trans.abort();
            goto RETRY;
        }
    }
    return;
}

int main(int argc, char *argv[])
{
    print_mode();
    // displayParameter();
    makeDB();
    chrono::system_clock::time_point starttime, endtime;

    bool start = false;
    bool quit = false;
    initResult();
    std::vector<char> readys(thread_num);

    std::vector<std::thread> thv;
    for (size_t i = 0; i < thread_num; ++i)
    {
        thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                         std::ref(quit));
    }
    waitForReady(readys);

    starttime = chrono::system_clock::now();
    start = true;
    this_thread::sleep_for(std::chrono::milliseconds(1000 * extime));
    quit = true;

    for (auto &th : thv)
    {
        th.join();
    }
    endtime = chrono::system_clock::now();

    double time = static_cast<double>(chrono::duration_cast<chrono::microseconds>(endtime - starttime).count() / 1000.0);
    // printf("time %lf[ms]\n", time);

    for (unsigned int i = 0; i < thread_num; ++i)
    {
        ErmiaResult[0].addLocalAllResult(ErmiaResult[i]);
    }
    ErmiaResult[0].displayAllResult();

    uint64_t result = (ErmiaResult[0].total_commit_counts_ * 1000) / time;
    cout << "latency[ns]:\t\t\t" << powl(10.0, 9.0) / result * thread_num
         << endl;
    cout << "throughput[tps]:\t\t" << result << endl;

    std::quick_exit(0);

    return 0;
}
