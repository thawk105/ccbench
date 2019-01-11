#include <ctype.h>	//isdigit, 
#include <iostream>
#include <pthread.h>
#include <string.h>	//strlen,
#include <string>	//string
#include <sys/syscall.h>	//syscall(SYS_gettid),	
#include <sys/types.h>	//syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>	//syscall(SYS_gettid), 

#define GLOBAL_VALUE_DEFINE
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/int64byte.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"
#include "include/garbageCollection.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayAbortCounts();
extern void displayAbortRate();
extern void displayDB();
extern void displayPRO();
extern void displayTPS(uint64_t &bgn, uint64_t &end);
extern void deceideTMTgcThreshold();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void naiveGarbageCollection();
extern inline uint64_t rdtsc();
extern void waitForReadyOfAllThread();

static bool
chkInt(const char *arg)
{
	for (unsigned int i = 0; i < strlen(arg); ++i) {
		if (!isdigit(arg[i])) {
			cout << std::string(arg) << " is not a number." << endl;
			exit(0);
		}
	}
	return true;
}

static void
chkArg(const int argc, const char *argv[])
{
	if (argc != 7) {
	//if (argc != 1) {
		cout << "usage: ./ermia.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO CPU_MHZ EXTIME" << endl;
		cout << "example: ./ermia.exe 200 10 24 3 2400 3" << endl;
		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "RRATIO: read ratio (* 10%%)" << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "Version " << sizeof(Version) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
		cout << "TransactionTable " << sizeof(TransactionTable) << endl;

		exit(0);
	}

	//test
	//TUPLE_NUM = 10;
	//MAX_OPE = 10;
	//THREAD_NUM = 2;
	//PRO_NUM = 100;
	//READ_RATIO = 0.5;
	//CLOCK_PER_US = 2400;
	//EXTIME = 3;
	//-----
	
	
	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	chkInt(argv[6]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	if (THREAD_NUM < 2) {
		cout << "1 thread is leader thread. \nthread number 1 is no worker thread, so exit." << endl;
		ERR;
	}
	RRATIO = atoi(argv[4]);
	if (RRATIO > 10) {
		cout << "rratio (* 10 %%) must be 0 ~ 10" << endl;
		ERR;
	}

	CLOCK_PER_US = atof(argv[5]);
	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}

	EXTIME = atoi(argv[6]);

	try {
		FinishTransactions = new uint64_t[THREAD_NUM];
		AbortCounts = new uint64_t[THREAD_NUM];
		TMT = new TransactionTable*[THREAD_NUM];
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		FinishTransactions[i] = 0;
		AbortCounts[i] = 0;
		TMT[i] = new TransactionTable(0, 0, UINT32_MAX, 0, TransactionStatus::inFlight);
	}
}

static void *
manager_worker(void *arg)
{
	// start, inital work
	int *myid = (int *)arg;
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;
	GarbageCollection gcobject;
	
	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		ERR;
	}

	gcobject.decideFirstRange();
	waitForReadyOfAllThread();
	// end, initial work
	
	
	Bgn = rdtsc();
	for (;;) {
		usleep(1);
		End = rdtsc();
		if (chkClkSpan(Bgn, End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
			Finish.store(true, std::memory_order_release);
			return nullptr;
		}

		//naiveGarbageCollection()
		
		if (gcobject.chkSecondRange()) {
			gcobject.decideGcThreshold();
			gcobject.mvSecondRangeToFirstRange();
		}
	}

	return nullptr;
}


static void *
worker(void *arg)
{
	int *myid = (int *)arg;
	Transaction trans(*myid, MAX_OPE);
	Procedure pro[MAX_OPE];
	Xoroshiro128Plus rnd;
	rnd.init();
	uint64_t localFinishTransactions(0), localAbortCounts(0);

	//----------
	pid_t pid;
	cpu_set_t cpu_set;

	pid = syscall(SYS_gettid);
	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		ERR;
	}
	//check-test
	//printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
	//printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
	//----------
	
	//-----
	
	waitForReadyOfAllThread();
	
	//start work (transaction)
	try {
		for(;;) {
			makeProcedure(pro, rnd);
			asm volatile ("" ::: "memory");
RETRY:

			if (Finish.load(std::memory_order_acquire)) {
				CtrLock.w_lock();
				FinishTransactions[*myid] = localFinishTransactions;
				AbortCounts[*myid] = localAbortCounts;
				CtrLock.w_unlock();
				return nullptr;
			}

			//-----
			//transaction begin
			trans.tbegin();
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.ssn_tread(pro[i].key);
					//if (trans.status == TransactionStatus::aborted) NNN;
				} else if (pro[i].ope == Ope::WRITE) {
					trans.ssn_twrite(pro[i].key, pro[i].val);
					//if (trans.status == TransactionStatus::aborted) NNN;
				} else {
					ERR;
				}

				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
					localAbortCounts++;
					goto RETRY;
				}
			}
			
			trans.ssn_parallel_commit();
			if (trans.status == TransactionStatus::aborted) {
				trans.abort();
				localAbortCounts++;
				goto RETRY;
			}
			localFinishTransactions++;

			uint32_t loadThreshold = trans.gcobject.getGcThreshold();
			if (trans.preGcThreshold != loadThreshold) {
				trans.gcobject.gcTMTelement(loadThreshold);
			}
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
		if (pthread_create(&t, nullptr, manager_worker, (void *)myid)) ERR;
	} else {
		if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
	}

	return t;
}

int
main(const int argc, const char *argv[])
{
	chkArg(argc, argv);
	makeDB();

	//displayDB();
	//displayPRO();

	pthread_t thread[THREAD_NUM];

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		thread[i] = threadCreate(i);
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		pthread_join(thread[i], nullptr);
	}

	//displayDB();

	displayTPS(Bgn, End);
	displayAbortCounts();
	//displayAbortRate();

	return 0;
}
