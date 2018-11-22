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

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void displayFinishTransactions();
extern void displayAbortCounts();
extern void displayAbortRate();
extern void displayRtsudRate();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void prtRslt(uint64_t &bgn, uint64_t &end);
extern void sumTrans(Transaction *trans);

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
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM RRATIO CLOCK_PER_US EXTIME\n\
\n\
example:./main 200 20 15 3 2400 3\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of thread.\n\
RRATIO : read ratio (* 10%%)\n\
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
	RRATIO = atoi(argv[4]);
	if (RRATIO > 10) {
		cout << "rratio must be 0 ~ 10" << endl;
		ERR;
	}

	CLOCK_PER_US = atof(argv[5]);
	EXTIME = atoi(argv[6]);
}

static void *
worker(void *arg)
{
	const int *myid = (int *)arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	Transaction trans(*myid);

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
	unsigned int expected, desired;
	for (;;) {
		expected = Running.load(memory_order_acquire);
RETRY_WAIT:
		desired = expected + 1;
		if (Running.compare_exchange_strong(expected, desired, memory_order_acq_rel, memory_order_acquire)) break;
		else goto RETRY_WAIT;
	}
	
	//spin wait
	while (Running != THREAD_NUM);
	//----------
	
	if (*myid == 0) Bgn = rdtsc();

	try {
		//start work(transaction)
		for (;;) {
			if (*myid == 0) {
				End = rdtsc();
				if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
					Finish.store(true, std::memory_order_release);
					sumTrans(&trans);
					return nullptr;
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					sumTrans(&trans);
					return nullptr;
				}
			}

			//transaction begin
			//Read phase
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
				trans.writePhase();
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
	//displayFinishTransactions();

	prtRslt(Bgn, End);
	displayRtsudRate();
	//displayAbortRate();
	//displayAbortCounts();

	return 0;
}
