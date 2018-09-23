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
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/int64byte.hpp"
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

	switch (atoi(argv[4])) {
		case 0:
			WORKLOAD = Workload::R_ONLY;
			break;
		case 1:
			WORKLOAD = Workload::R_INTENS;
			break;
		case 2:
			WORKLOAD = Workload::RW_EVEN;
			break;
		case 3:
			WORKLOAD = Workload::W_INTENS;
			break;
		case 4:
			WORKLOAD = Workload::W_ONLY;
			break;
		default:
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
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(std::atomic<uint64_t>)) != 0) ERR;
		if (posix_memalign((void**)&ThRecentTID, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Rnd, 64, THREAD_NUM * sizeof(Xoroshiro128Plus)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		FinishTransactions[i].num = 0;
		AbortCounts[i].num = 0;
		ThRecentTID[i].num = 0;
		ThLocalEpoch[i] = 0;
		Rnd[i].init();
	}
}

static void 
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sumTrans += FinishTransactions[i].num;
	}

	uint64_t result = (double) sumTrans / (double) sec;
	cout << (int)result << endl;
}

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
void threadEndProcess(int *myid);

bool
chkEpochLoaded()
{
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		if (ThLocalEpoch[i].load(memory_order_acquire) != GlobalEpoch) return false;
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
	//uint64_t TempTimerStart, TempTimerStop;
	// temprature reset timer

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		printf("thread affinity setting is error.\n");
		exit(1);
	}

	//----------
	//wait for all threads start. CAS.
	unsigned int expected;
	unsigned int desired;
	do {
		expected = Running.load(memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, memory_order_acq_rel));

	//spin wait
	while (Running.load(memory_order_acquire) != THREAD_NUM) {};
	//----------

	EpochTimerStart = rdtsc();
	while (Ending.load(memory_order_acquire) != THREAD_NUM - 1) {
		//Epoch Control
		EpochTimerStop = rdtsc();
		//chkEpochLoaded は最新のグローバルエポックを
		//全てのワーカースレッドが読み込んだか確認する．
		if (chkClkSpan(EpochTimerStart, EpochTimerStop, EPOCH_TIME * CLOCK_PER_US * 1000) && chkEpochLoaded()) {
			GlobalEpoch++;
		
			//Reset temprature
			for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
				Table[i].temp.store(0, memory_order_release);
			}
			
			EpochTimerStart = rdtsc();
		}
		//----------
	}
	
	return nullptr;
}

extern void makeProcedure(Procedure *pro, unsigned int thid);

static void *
worker(void *arg)
{
	int *myid = (int *) arg;
	Procedure pro[MAX_OPE];

	//----------
	pid_t pid = gettid();
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		printf("thread affinity setting is error.\n");
		exit(1);
	}

	//----------
	//wait for all threads start. CAS.
	unsigned int expected;
	unsigned int desired;
	do {
		expected = Running.load(memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, memory_order_acq_rel));
	
	//spin wait
	while (Running != THREAD_NUM) {}
	//----------
	
	//start work (transaction)
	if (*myid == 1) Bgn = rdtsc();

	Transaction trans(*myid);
	try {
		for (;;) {
			if (*myid == 1) {
				//if (FinishTransactions[*myid].num % 1000 == 0) {
					End = rdtsc();
					if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
						Finish.store(true, memory_order_release);
						do {
							expected = Ending.load(memory_order_acquire);
							desired = expected + 1;
						} while (!Ending.compare_exchange_weak(expected, desired, memory_order_acq_rel));
						return nullptr;
					}
				//}
			} else {
				if (Finish.load(memory_order_acquire)) {
					do {
						expected = Ending.load(memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, memory_order_acq_rel));
					return nullptr;
				}
			}

			makeProcedure(pro, *myid);
			asm volatile ("" ::: "memory");
RETRY:
			trans.begin();
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.read(pro[i].key);
				} else {
					trans.write(pro[i].key, pro[i].val);
				}
				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
					goto RETRY;
				}
			}

			if (!(trans.commit())) {
				trans.abort();
				goto RETRY;
			}

			trans.writePhase();

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

	return 0;
}
