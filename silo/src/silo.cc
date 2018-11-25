#include <cctype>
#include <ctype.h>
#include <algorithm>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>

#define GLOBAL_VALUE_DEFINE
#include "include/atomic_tool.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern bool chkEpochLoaded();
extern void displayDB();
extern void displayPRO();
extern void displayFinishTransactions();
extern void displayAbortCounts();
extern void displayTotalAbortCounts();
extern void displayAbortRate();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void prtRslt(uint64_t &bgn, uint64_t &end);
extern void sumTrans(Transaction *trans);

void threadEndProcess(int *myid);

static bool
chkInt(char *arg)
{
    for (uint i=0; i<strlen(arg); ++i) {
        if (!isdigit(arg[i])) {
			cout << std::string(arg) << " is not a number." << endl;
			exit(0);
        }
    }
	return true;
}

static void 
chkArg(const int argc, char *argv[])
{
	if (argc != 8) {
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM WORKLOAD EPOCH_TIME EXTIME\n\
\n\
example:./main 1000000 20 15 3 2400 40 3\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of thread.\n\
WORKLOAD:\n\
0. read only (read 100%%)\n\
1. read intensive (read 80%%)\n\
2. read write even (read 50%%)\n\
3. write intensive (write 80%%)\n\
4. write only (write 100%%)\n\
CLOCK_PER_US: CPU_MHZ\n\
EPOCH_TIME(int)(ms): Ex. 40\n\
EXTIME: execution time.\n\
\n");
	    cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
		exit(0);
	}
	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	chkInt(argv[5]);
	chkInt(argv[6]);
	chkInt(argv[7]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	WORKLOAD = atoi(argv[4]);
	if (WORKLOAD > 4) {
		ERR;
	}

	CLOCK_PER_US = atof(argv[5]);
	EPOCH_TIME = atoi(argv[6]);
	EXTIME = atoi(argv[7]);
	
	if (THREAD_NUM < 2) {
		printf("One thread is epoch thread, and others are worker threads.\n\
So you have to set THREAD_NUM >= 2.\n\n");
	}

	try {
		if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;	//[0]は使わない
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		ThLocalEpoch[i].obj = 0;
	}
}

static void *
epoch_worker(void *arg)
{
//1. 40msごとに global epoch を必要に応じてインクリメントする
//2. 十分条件
//	全ての worker が最新の epoch を読み込んでいる。
//
	const int *myid = (int *)arg;
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;
	uint64_t EpochTimerStart, EpochTimerStop;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);
	
	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		printf("thread affinity setting is error.\n");
		exit(1);
	}

	//----------
	//wait for all threads start. CAS.
	unsigned int expected, desired;
	for (;;) {
		expected = Running.load(std::memory_order_acquire);
RETRY_WAIT_L:
		desired = expected + 1;
		if (Running.compare_exchange_weak(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT_L;
	}
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//----------

	const uint64_t finish_time = EXTIME * 1000 * 1000 * CLOCK_PER_US;

	//----------
	Bgn = rdtsc();
	EpochTimerStart = rdtsc();
	for (;;) {
		usleep(1);
		End = rdtsc();
		if (chkClkSpan(Bgn, End, finish_time)) {
			Finish.store(true, std::memory_order_release);
			return nullptr;
		}

		EpochTimerStop = rdtsc();
		//chkEpochLoaded は最新のグローバルエポックを
		//全てのワーカースレッドが読み込んだか確認する．
		if (chkClkSpan(EpochTimerStart, EpochTimerStop, EPOCH_TIME * CLOCK_PER_US * 1000) && chkEpochLoaded()) {
			atomicAddGE();
			EpochTimerStart = EpochTimerStop;
		}
	}
	//----------

	return nullptr;
}

static void *
worker(void *arg)
{
	const int *myid = (int *)arg;
	//const int thid = *myid;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	Transaction trans(*myid);

	//----------
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		printf("thread affinity setting is error.\n");
		exit(1);
	}
	//check-test
	//printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
	//printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
	//----------
	

	//----------
	//wait for all threads start. CAS.
	unsigned int expected, desired;
	for (;;) {
		expected = Running.load(std::memory_order_acquire);
RETRY_WAIT_W:
		desired = expected + 1;
		if (Running.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT_W;
	}
	
	//spin wait
	while (Running != THREAD_NUM) {}
	//----------
	
	try {
		//start work(transaction)
		for (;;) {
			makeProcedure(pro, rnd);
RETRY:
			trans.tbegin();
			if (Finish.load(memory_order_acquire)) {
				sumTrans(&trans);
				return nullptr;
			}

			//Read phase
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				switch(pro[i].ope) {
					case(Ope::READ):
						trans.tread(pro[i].key);
						break;
					case(Ope::WRITE):
						trans.twrite(pro[i].key, pro[i].val);
						break;
					default:
						ERR;
				}
			}
			
			//Validation phase
			if (trans.validationPhase()) {
				trans.writePhase();
			} else {
				trans.abort();
				goto RETRY;
			}

		}
	} catch (bad_alloc) {
		ERR;
	}

	return NULL;
}

pthread_t
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
		if (pthread_create(&t, NULL, epoch_worker, (void *)myid)) ERR;
	} else {
		if (pthread_create(&t, NULL, worker, (void *)myid)) ERR;
	}

	return t;
}

int 
main(int argc, char *argv[]) {
	chkArg(argc, argv);
	makeDB();
	
	//displayDB();
	//displayPRO();

	pthread_t thread[THREAD_NUM];

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		thread[i] = threadCreate(i);
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		pthread_join(thread[i], NULL);
	}

	//displayDB();
	//displayFinishTransactions();

	prtRslt(Bgn, End);
	displayAbortRate();
	//displayTotalAbortCounts();

	return 0;
}
