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
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

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
	if (argc != 7) {
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM WORKLOAD CLOCK_PER_US EXTIME\n\
\n\
example:./main 1000000 20 15 3 2400 3\n\
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
EXTIME: execution time.\n\
\n\n");
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
	EXTIME = atoi(argv[6]);

	try {
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		AbortCounts[i].num = 0;
		FinishTransactions[i].num = 0;
	}
}

static void 
prtRslt(uint64_t &bgn, uint64_t &end)
{
	/*
	long usec;
	double sec;

	usec = (end.tv_sec - bgn.tv_sec) * 1000 * 1000 + (end.tv_usec - bgn.tv_usec);
	sec = (double) usec / 1000.0 / 1000.0;

	double result = (double)sumTrans / sec;
	//cout << Finish_transactions.load(std::memory_order_acquire) << endl;
	cout << (int)result << endl;
	*/

	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sumTrans += FinishTransactions[i].num;
	}

	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);

static void *
worker(void *arg)
{
	int *myid = (int *)arg;
	Procedure pro[MAX_OPE];
	Xoroshiro128Plus rnd;
	rnd.init();

	//----------
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);
	//epoch_worker が存在するので， + 1 をする．

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
	unsigned int expected;
	unsigned int desired;
	do {
		expected = Running.load(std::memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
	
	//spin wait
	while (Running != THREAD_NUM) {
	}
	//----------
	
	//start work(transaction)
	if (*myid == 0) Bgn = rdtsc();

	try {

		Transaction trans(*myid);
		for (;;) {
			if (*myid == 0) {
				End = rdtsc();
				if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
					Finish.store(true, std::memory_order_release);

					do {
						expected = Ending.load(std::memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
					return nullptr;
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					do {
						expected = Ending.load(std::memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
					return nullptr;
				}
			}

			//transaction begin

			//Read phase
			//Search versions
			makeProcedure(pro, rnd);
			asm volatile ("" ::: "memory");
RETRY:
			trans.tbegin();
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.tread(pro[i].key);
					if (trans.status == TransactionStatus::aborted) {
						trans.abort();
						goto RETRY;
					}
				} else if (pro[i].ope == Ope::WRITE) {
					trans.twrite(pro[i].key, pro[i].val);
				} else {
					ERR;
				}
			}
			
			//Validation phase
			if (trans.validationPhase()) {
				//Write phase
				trans.writePhase();
				//cout << "commit" << endl;
			} else {
				trans.abort();
				goto RETRY;
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

	if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;

	return t;
}

extern void displayDB();
extern void displayPRO();
extern void displayFinishTransactions();
extern void displayAbortCounts();
extern void displayAbortRate();
extern void makeDB();

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
		pthread_join(thread[i], nullptr);
	}

	//displayDB();
	//displayAbortRate();
	//displayFinishTransactions();
	//displayAbortCounts();

	prtRslt(Bgn, End);

	return 0;
}
