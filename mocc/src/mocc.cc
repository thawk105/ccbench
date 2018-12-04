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
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

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
	if (argc != 8) {
		cout << "usage: ./mocc.exe TUPLE_NUM MAX_OPE THREAD_NUM WORKLOAD CPU_MHZ EPOCH_TIME EXTIME" << endl;
		cout << "example: ./mocc.exe 200 10 24 3 2400 40 3" << endl;

		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "WORKLOAD:" << endl;
	    cout << "0. read only (read 100%%)" << endl;
		cout << "1. read intensive (read 80%%)" << endl;
		cout << "2. read write even (read 50%%)" << endl;
		cout << "3. write intensive (write 80%%)" << endl;
		cout << "4. write only (write 100%%)" << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EPOCH_TIME(unsigned int)(ms): Ex. 40" << endl;
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "RWLock " << sizeof(RWLock) << endl;
		cout << "MQLock " << sizeof(MQLock) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
		cout << "Xoroshiro128Plus " << sizeof(Xoroshiro128Plus) << endl;

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
		cout << "workload must be 0 ~ 4" << endl;
		ERR;
	}

	CLOCK_PER_US = atof(argv[5]);
	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}

	EPOCH_TIME = atoi(argv[6]);
	EXTIME = atoi(argv[7]);

	try {
		FinishTransactions = new uint64_t[THREAD_NUM];
		AbortCounts = new uint64_t[THREAD_NUM];
		if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		//if (posix_memalign((void**)MQL_node_list, 64, THREAD_NUM * sizeof(MQLnode)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		FinishTransactions[i] = 0;
		AbortCounts[i] = 0;
		ThLocalEpoch[i].obj = 0;
	}
}

static void 
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sumTrans += FinishTransactions[i];
	}

	uint64_t result = (double) sumTrans / (double) sec;
	cout << (int)result << endl;
}

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
void threadEndProcess(int *myid);

bool
chkEpochLoaded()
{
	uint64_t_64byte nowepo = loadAcquireGE();
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		if (__atomic_load_n(&(ThLocalEpoch[i].obj), __ATOMIC_ACQUIRE) != nowepo.obj) return false;
	}

	return true;
}

pid_t gettid(void)
{
	return syscall(SYS_gettid);
}

static void *
epoch_worker(void *arg)
{
	int *myid = (int *)arg;
	pid_t pid = gettid();
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
		expected = Running.load(memory_order_acquire);
RETRY_WAIT_L:
		desired = expected + 1;
		if (Running.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT_L;
	}

	//spin wait
	while (Running.load(memory_order_acquire) != THREAD_NUM) {};
	//----------

	uint64_t finish_time = EXTIME * 1000 * 1000 * CLOCK_PER_US;
	Bgn = rdtsc();
	EpochTimerStart = rdtsc();
	for (;;) {
		usleep(1);
		End = rdtsc();
		if (chkClkSpan(Bgn, End, finish_time)) {
			Finish.store(true, memory_order_release);
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

extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);

static void *
worker(void *arg)
{
	int *myid = (int *) arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	unsigned int expected, desired;
	uint64_t totalFinishTransactions(0), totalAbortCounts(0);
	Transaction trans(*myid, &rnd);

	//----------
	pid_t pid = gettid();
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) ERR;
	//----------
	//wait for all threads start. CAS.
	for (;;) {
		expected = Running.load(memory_order_acquire);
RETRY_WAIT_W:
		desired = expected + 1;
		if (Running.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT_W;
	}
	
	//spin wait
	while (Running.load(memory_order_acquire) != THREAD_NUM) {}
	//----------
	
	try {
		//start work (transaction)
		for (;;) {
			makeProcedure(pro, rnd);
			asm volatile ("" ::: "memory");
RETRY:
			if (Finish.load(memory_order_acquire)) {
				CtrLock.w_lock();
				FinishTransactions[*myid] = totalFinishTransactions;
				AbortCounts[*myid] = totalAbortCounts;
				CtrLock.w_unlock();
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
					totalAbortCounts++;
					goto RETRY;
				}
			}

			if (!(trans.commit())) {
				trans.abort();
				totalAbortCounts++;
				goto RETRY;
			}

			trans.writePhase();
			totalFinishTransactions++;

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

extern void displayDB();
extern void displayPRO();
extern void displayAbortRate();
extern void makeDB();

int
main(int argc, char *argv[]) 
{
	chkArg(argc, argv);
	makeDB();

	//displayDB();

	pthread_t thread[THREAD_NUM];

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		thread[i] = threadCreate(i);
	}
	
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		pthread_join(thread[i], nullptr);
	}

	prtRslt(Bgn, End);
	displayAbortRate();

	return 0;
}
