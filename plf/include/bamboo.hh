#include <iostream>

enum lock_type
{
    SH = -1,
    EX = 1
};

class Bamboo
{
private:
    /* data */
public:
    Bamboo(/* args */);
    ~Bamboo();
    bool conflict(int t1, int t2, uint64_t key);
};

Bamboo::Bamboo(/* args */)
{
}

Bamboo::~Bamboo()
{
}

void TxExecutor::LockAcquire(uint64_t key)
{
  int t;
  bool has_conflicts = false;
  bool already_acquired = false;
  Tuple *tuple;
  tuple = get_tuple(Table, key);
  LockType lock_type = (LockType)tuple->req_type[thid_];
  LockType type;
  while (1)
  {
    if (tuple->lock_.w_trylock())
    {
      printf("tx%d lock acquire\n", thid_);
      has_conflicts = false;
      isConflict(tuple->retired, lock_type, type);
      for (int i = 0; i < tuple->retired.size(); i++)
      {
        t = tuple->retired[i];
        type = (LockType)tuple->req_type[t];
        if (conflict(lock_type, type))
        {
          has_conflicts = true;
        }
        else
        {
          has_conflicts = false;
        }
        if (has_conflicts == true && thread_timestamp[thid_] < thread_timestamp[t])
        {
          thread_stats[t];
        }
      }
      for (int i = 0; i < tuple->owners->owner_list.size(); i++)
      {
        t = tuple->owners->owner_list[i]->first;
        type = tuple->owners->owner_list[i]->second;
        if (conflict(lock_type, type))
        {
          has_conflicts = true;
        }
        else
        {
          has_conflicts = false;
        }
        if (has_conflicts == true && thread_timestamp[thid_] < thread_timestamp[t])
        {
          printf("tx%d aborts tx%d\n", txn->ts, t->ts);
          thread_stats[t];
        }
      }
      pair<TX *, req_type> *waiter_txn = new pair<TX *, req_type>;
      waiter_txn->first = txn;
      waiter_txn->second = lock_type;
      tuple->waiters->add(waiter_txn);
      // printf("tuple->waiters.wait_list size is %lu\n", tuple->waiters->wait_list.size());
      printf("tx%d PromoteWaiters\n", txn->ts);
      PromoteWaiters(tuple);
      unLock(tuple->acquire_lock);
      return;
    }
    else
    {
      usleep(1);
    }
  }
}