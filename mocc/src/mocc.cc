#include <ctype.h>	// isdigit,
#include <iostream>
#include <pthread.h>
#include <string.h>	// strlen,
#include <string>	// string
#include <sys/syscall.h>	// syscall(SYS_gettid),
#include <sys/types.h>	// syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>	// syscall(SYS_gettid),

#define GLOBAL_VALUE_DEFINE
#include "../../include/int64byte.hpp"
#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"
#include "include/zipf.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

static bool
chkInt(const char *arg)
{
	for (unsigned int i = 0; i < strlen(arg); ++i) {
		if (!isdigit(arg[i])) {
			cout << string(arg) << "is not a number." << endl;
			exit(0);
		}
	}
	
	return true;
}

static void
chkArg(const int argc, char *argv[])
{
	if (argc != 10) {
		cout << "usage: ./mocc.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO ZIPF_SKEW YCSB CPU_MHZ EPOCH_TIME EXTIME" << endl;
		cout << "example: ./mocc.exe 200 10 24 50 0 ON 2400 40 3" << endl;

		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "RRATIO : read ratio [%%]" << endl;
		cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
		cout << "YCSB : ON or OFF. switch makeProcedure function." << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EPOCH_TIME(unsigned int)(ms): Ex. 40" << endl;
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "RWLock " << sizeof(RWLock) << endl;
		cout << "MQLock " << sizeof(MQLock) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
		cout << "Xoroshiro128Plus " << sizeof(Xoroshiro128Plus) << endl;
		cout << "pthread_mutex_t" << sizeof(pthread_mutex_t) << endl;

		exit(0);
	}


	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	chkInt(argv[7]);
	chkInt(argv[8]);
	chkInt(argv[9]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	RRATIO = atoi(argv[4]);
  ZIPF_SKEW = atof(argv[5]);
  string argycsb = argv[6];
	CLOCK_PER_US = atof(argv[7]);
	EPOCH_TIME = atoi(argv[8]);
	EXTIME = atoi(argv[9]);

	if (RRATIO > 100) {
		cout << "rratio (* 10 \%) must be 0 ~ 10)" << endl;
		ERR;
	}

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argycsb == "ON")
    YCSB = true;
  else if (argycsb == "OFF")
    YCSB = false;
  else ERR;

	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}

	try {
		if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
#ifdef MQLOCK
		//if (posix_memalign((void**)&MQLNodeList, 64, (THREAD_NUM + 3) * sizeof(MQLNode)) != 0) ERR;
		MQLNodeTable = new MQLNode*[THREAD_NUM + 3];
		for (unsigned int i = 0; i < THREAD_NUM + 3; ++i)
			MQLNodeTable[i] = new MQLNode[TUPLE_NUM];
#endif // MQLOCK
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		ThLocalEpoch[i].obj = 0;
	}
}

bool
chkEpochLoaded()
{
	uint64_t_64byte nowepo = loadAcquireGE();
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		if (__atomic_load_n(&(ThLocalEpoch[i].obj), __ATOMIC_ACQUIRE) != nowepo.obj) return false;
	}

	return true;
}

static void *
epoch_worker(void *arg)
{
	int *myid = (int *)arg;
	uint64_t EpochTimerStart, EpochTimerStop;
  Result rsobject;

  setThreadAffinity(*myid);
  waitForReadyOfAllThread();

	rsobject.Bgn = rdtsc();
	EpochTimerStart = rdtsc();
	for (;;) {
		usleep(1);
		rsobject.End = rdtsc();
		if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
			rsobject.Finish.store(true, memory_order_release);
			return nullptr;
		}

		//Epoch Control
		EpochTimerStop = rdtsc();
		//chkEpochLoaded は最新のグローバルエポックを
		//全てのワーカースレッドが読み込んだか確認する．
		if (chkClkSpan(EpochTimerStart, EpochTimerStop, EPOCH_TIME * CLOCK_PER_US * 1000) && chkEpochLoaded()) {
			atomicAddGE();
			EpochTimerStart = EpochTimerStop;
		}
		//----------
	}
	
	return nullptr;
}

static void *
worker(void *arg)
{
	int *myid = (int *) arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
  Result rsobject;
	int locknum = *myid + 2;
	// 0 : None
	// 1 : Acquired
	// 2 : SuccessorLeaving
	// 3 : th num 1
	// 4 : th num 2
	// ..., th num 0 is leader thread.
	Transaction trans(*myid, &rnd, locknum);
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

  setThreadAffinity(*myid);
  waitForReadyOfAllThread();
	
	try {
		//start work (transaction)
		for (;;) {
      if (YCSB)
			  makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

			asm volatile ("" ::: "memory");
RETRY:
			if (rsobject.Finish.load(memory_order_acquire)) {
        rsobject.sumUpAbortCounts();
        rsobject.sumUpCommitCounts();
				return nullptr;
			}

			trans.begin();
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.read(pro[i].key);
				} else {
					trans.write(pro[i].key, pro[i].val);
				}
				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
          ++rsobject.localAbortCounts;
					goto RETRY;
				}
			}

			if (!(trans.commit())) {
				trans.abort();
        ++rsobject.localAbortCounts;
				goto RETRY;
			}

			trans.writePhase();
      ++rsobject.localCommitCounts;

		}
	} catch (bad_alloc) {
		ERR;
	}

	return nullptr;
}

static pthread_t
threadCreate(int id)
{
	pthread_t t;
	int *myid;

	try {
		myid = new int;
	} catch (bad_alloc) {
		ERR;
	}
	*myid = id;

	if (*myid == 0) {
		if (pthread_create(&t, nullptr, epoch_worker, (void *)myid)) ERR;
	} else {
		if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
	}

	return t;
}

int
main(int argc, char *argv[]) 
{
  Result rsobject;
	chkArg(argc, argv);
	makeDB();

	pthread_t thread[THREAD_NUM];

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		thread[i] = threadCreate(i);
	}
	
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		pthread_join(thread[i], nullptr);
	}

  rsobject.displayTPS();
	rsobject.displayAbortRate();

	return 0;
}
