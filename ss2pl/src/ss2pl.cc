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
	if (argc != 8) {
		cout << "usage: ./ss2pl.exe TUPLE_NUM MAX_OPE THREAD_NUM PRO_NUM READ_RATIO CPU_MHZ EXTIME" << endl;
		cout << "example: ./ss2pl.exe 200 10 24 10000 0.5 2400 3" << endl;
		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "PRO_NUM(int): Initial total numbers of transactions" << endl;
		cout << "READ_RATIO(float): ratio of read in transaction" << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple size " << sizeof(Tuple) << endl;
		cout << "std::mutex size " << sizeof(mutex) << endl;
		exit(0);
	}

	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	PRO_NUM = atoi(argv[4]);
	READ_RATIO = atof(argv[5]);

	CLOCK_PER_US = atof(argv[6]);
	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}

	chkInt(argv[7]);
	EXTIME = atoi(argv[7]);

	if (THREAD_NUM > PRO_NUM) {
		cout << "THREAD_NUM must be smaller than PRO_NUM" << endl;
		ERR;
	}

	try {
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts2, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		AbortCounts[i].num = 0;
		AbortCounts2[i].num = 0;
		FinishTransactions[i].num = 0;
	}
}

static void
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (int i = 0; i < THREAD_NUM; i++) {
		sumTrans += FinishTransactions[i].num;
	}

	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}

static void *
worker(void *arg)
{

	int *myid = (int *)arg;
	//----------
	pid_t pid;
	cpu_set_t cpu_set;
	int result;

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
	int expected, desired;
	do {
		expected = Running.load();
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired));
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//-----
	
	//start work (transaction)
	if (*myid == 0) Bgn = rdtsc();

	try {
		for (int i = PRO_NUM / THREAD_NUM * (*myid); i < PRO_NUM / THREAD_NUM * (*myid + 1); i++) {
RETRY:
			//End judgment
			if (*myid == 0) {
				if (FinishTransactions[*myid].num % 1000 == 0 || AbortCounts[*myid].num % 1000 == 0) {
					End = rdtsc();
					if (chkClkSpan(Bgn, End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
						Finish.store(true, std::memory_order_release);
						return NULL;
					}
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					do {
						expected = Ending.load(std::memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
					return NULL;
				}
			}
			//-----
			
			Transaction trans;
			trans.tbegin(*myid);
			//transaction begin
			
			for (int j = 0; j < MAX_OPE; j++) {
				int value_read;
				switch (Pro[i][j].ope) {
					case (Ope::READ) :
						value_read = trans.tread(Pro[i][j].key);
						break;
					case (Ope::WRITE) :
						trans.twrite(Pro[i][j].key, Pro[i][j].val);
						break;
					default:
						break;
				}
				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
					goto RETRY;
				}
			}

			//commit - write phase
			trans.commit();

			if (i == (PRO_NUM / THREAD_NUM * (*myid + 1)) - 1) {
				i = PRO_NUM / THREAD_NUM * (*myid);
			}
		}
	} catch (bad_alloc) {
		ERR;
	}

	return NULL;
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

	if (pthread_create(&t, NULL, worker, (void *)myid)) ERR;

	return t;
}

extern void displayDB();
extern void displayPRO();
extern void displayAbortRate();
extern void makeProcedure();
extern void makeDB();

int
main(const int argc, const char *argv[])
{
	chkArg(argc, argv);
	makeDB();
	makeProcedure();

	//displayDB();
	//displayPRO();

	pthread_t thread[THREAD_NUM];

	for (int i = 0; i < THREAD_NUM; i++) {
		thread[i] = threadCreate(i);
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(thread[i], NULL);
	}

	prtRslt(Bgn, End);
	//displayAbortRatio();

	return 0;
}
