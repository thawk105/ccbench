#include <pthread.h>
#include <string.h>	//strlen,
#include <sys/types.h>	//syscall(SYS_gettid),
#include <sys/syscall.h>	//syscall(SYS_gettid),	
#include <thread>
#include <unistd.h>	//syscall(SYS_gettid), 
#include <ctype.h>	//isdigit, 
#include <string>	//string
#include <iostream>

#define GLOBAL_VALUE_DEFINE
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/int64byte.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

static bool
chkInt(const char *arg)
{
	for (unsigned int i = 0; i < strlen(arg); i++) {
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
		cout << "usage: ./ss2pl.exe TUPLE_NUM MAX_OPE THREAD_NUM WORKLOAD CPU_MHZ EXTIME" << endl;
		cout << "example: ./ss2pl.exe 200 10 24 3 2400 3" << endl;
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
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple size " << sizeof(Tuple) << endl;
		cout << "std::mutex size " << sizeof(mutex) << endl;
		cout << "RWLock size " << sizeof(RWLock) << endl;
		exit(0);
	}

	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	chkInt(argv[5]);
	chkInt(argv[6]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	WORKLOAD = atoi(argv[4]);
	if (WORKLOAD > 4) {
		ERR;
	}

	CLOCK_PER_US = atof(argv[5]);
	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}

	EXTIME = atoi(argv[6]);

	try {
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		FinishTransactions[i] = 0;
		AbortCounts[i] = 0;
	}
}

static void
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		sumTrans += FinishTransactions[i];
	}

	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}

extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);

static void *
worker(void *arg)
{

	const int *myid = (int *)arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	uint64_t totalFinishTransactions(0), totalAbortCounts(0);
	Transaction trans(*myid);

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
	unsigned int expected, desired;
	for (;;) {
		expected = Running.load();
RETRY_WAIT:
		desired = expected + 1;
		if (Running.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT;
	}
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//-----
	
	if (*myid == 0) Bgn = rdtsc();

	try {
		//start work (transaction)
		for (;;) {
			//End judgment
			if (*myid == 0) {
				End = rdtsc();
				if (chkClkSpan(Bgn, End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
					Finish.store(true, std::memory_order_release);
					CtrLock.w_lock();
					FinishTransactions[*myid] = totalFinishTransactions;
					AbortCounts[*myid] = totalAbortCounts;
					CtrLock.w_unlock();
					return nullptr;
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					CtrLock.w_lock();
					FinishTransactions[*myid] = totalFinishTransactions;
					AbortCounts[*myid] = totalAbortCounts;
					CtrLock.w_unlock();
					return nullptr;
				}
			}
			//-----
			
			makeProcedure(pro, rnd);
RETRY:
			trans.tbegin();
			//transaction begin
			
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.tread(pro[i].key);
				} else {
					trans.twrite(pro[i].key, pro[i].val);
				}
				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
					totalAbortCounts++;
					goto RETRY;
				}
			}

			//commit - write phase
			trans.commit();
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

	if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;

	return t;
}

extern void displayDB();
extern void displayPRO();
extern void displayAbortRate();
extern void makeDB();

int
main(const int argc, const char *argv[])
{
	chkArg(argc, argv);
	makeDB();

	//displayDB();
	//displayPRO();

	pthread_t thread[THREAD_NUM];

	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		thread[i] = threadCreate(i);
	}

	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		pthread_join(thread[i], nullptr);
	}

	prtRslt(Bgn, End);
	//displayAbortRate();

	return 0;
}
