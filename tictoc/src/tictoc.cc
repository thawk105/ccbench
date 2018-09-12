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
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

static bool
chkInt(char *arg)
{
    for (uint i=0; i<strlen(arg); i++) {
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
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM PRO_NUM READ_RATIO(0~1) EXTIME\n\
\n\
example:./main 1000000 20 15 10000 0.5 2400 40 3\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of thread.\n\
PRO_NUM(int):    initial total numbers of transactions.\n\
READ_RATIO(float): ratio of read in transaction.\n\
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
	chkInt(argv[6]);
	chkInt(argv[7]);
	chkInt(argv[8]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	PRO_NUM = atoi(argv[4]);
	READ_RATIO = atof(argv[5]);
	CLOCK_PER_US = atof(argv[6]);
	EXTIME = atoi(argv[7]);
	
	if (THREAD_NUM > PRO_NUM) {
		printf("THREAD_NUM must be smaller than PRO_NUM\n");
		exit(0);
	}
	if (THREAD_NUM < 2) {
		printf("One thread is epoch thread, and others are worker threads.\n\
So you have to set THREAD_NUM >= 2.\n\n");
	}

	try {
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;	//	use for logging
		if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;	//	use for logging
		if (posix_memalign((void**)&ThLocalEpoch, 64, THREAD_NUM * sizeof(std::atomic<uint64_t>)) != 0) ERR;	//[0]は使わない
		if (posix_memalign((void**)&ThRecentTID, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		AbortCounts[i].num = 0;
		ThRecentTID[i].num = 0;
		FinishTransactions[i].num = 0;
		ThLocalEpoch[i] = 0;
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
	for (unsigned int i = 0; i < THREAD_NUM; i++) {
		sumTrans += FinishTransactions[i].num;
	}

	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
void threadEndProcess(int *myid);

bool
chkEpochLoaded()
{
//全てのワーカースレッドが最新エポックを読み込んだか確認する．
	for (unsigned int i = 1; i < THREAD_NUM; i++) {
		if (ThLocalEpoch[i].load(std::memory_order_acquire) != GlobalEpoch) return false;
	}

	return true;
}

static void *
worker(void *arg)
{
	int *myid = (int *)arg;

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
		for (unsigned int i = PRO_NUM / (THREAD_NUM) * (*myid); i < PRO_NUM / (THREAD_NUM) * (*myid+1); ++i) {
RETRY:
			if (*myid == 0) {
				if (FinishTransactions[*myid].num % 1000 == 0) {
					End = rdtsc();
					if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
						Finish.store(true, std::memory_order_release);

						do {
							expected = Ending.load(std::memory_order_acquire);
							desired = expected + 1;
						} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
						FinishTransactions[*myid].num = localFinishTransactions;
						return NULL;
					}
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					do {
						expected = Ending.load(std::memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));
					FinishTransactions[*myid].num = localFinishTransactions;
					return NULL;
				}
			}

			//transaction begin

			//Read phase
			//Search versions
			for (unsigned int j = 0; j < MAX_OPE; ++j) {
				unsigned int value_read;
				switch(Pro[i][j].ope) {
					case(Ope::READ):
						value_read = trans.tread(Pro[i][j].key);
						//printf("read(%d) = %d\n", Pro[i][j].key, value_read);
						break;
					case(Ope::WRITE):
						trans.twrite(Pro[i][j].key, Pro[i][j].val);
						break;
					default:
						break;
				}
			}
			
			//Validation phase
			if (!(trans.validationPhase())) {
				trans.abort();
				goto RETRY;
			}

			//Write phase
			trans.writePhase();

			//Maintenance
			if (i == (PRO_NUM / (THREAD_NUM-1) * (*myid) - 1)) i = PRO_NUM / (THREAD_NUM-1) * (*myid - 1);
			localFinishTransactions++;
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

	if (*myid == 0) {
		if (pthread_create(&t, NULL, epoch_worker, (void *)myid)) ERR;
	} else {
		if (pthread_create(&t, NULL, worker, (void *)myid)) ERR;
	}

	return t;
}

extern void displayDB();
extern void displayPRO();
extern void displayFinishTransactions();
extern void displayAbortCounts();
extern void displayAbortRate();
extern void makeProcedure();
extern void makeDB();

int 
main(int argc, char *argv[]) {
	chkArg(argc, argv);
	makeDB();
	makeProcedure();
	
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
	//displayAbortRate();
	//displayFinishTransactions();
	//displayAbortCounts();

	prtRslt(Bgn, End);

	return 0;
}
