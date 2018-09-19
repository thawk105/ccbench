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
#include "include/transaction.hpp"
#include "include/tsc.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

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
	if (argc != 8) {
	//if (argc != 1) {
		cout << "usage: ./ermia.exe TUPLE_NUM MAX_OPE THREAD_NUM PRO_NUM READ_RATIO CPU_MHZ EXTIME" << endl;
		cout << "example: ./ermia.exe 200 10 24 10000 0.5 2400 3" << endl;
		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "PRO_NUM(int): Initial total numbers of transactions" << endl;
		cout << "READ_RATIO(float): ratio of read in transaction" << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EXTIME: execution time [sec]" << endl;

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "Version " << sizeof(Version) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;

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
	chkInt(argv[7]);

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

	EXTIME = atoi(argv[7]);

	if (THREAD_NUM > PRO_NUM) {
		cout << "THREAD_NUM must be smaller than PRO_NUM" << endl;
		ERR;
	}


	try {
		if (posix_memalign((void**)&ThtxID, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&TMT, 64, THREAD_NUM * sizeof(TransactionTable)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		ThtxID[i].num = 0;
		AbortCounts[i].num = 0;
		FinishTransactions[i].num = 0;

		TMT[i].cstamp.store(0, memory_order_release);
		TMT[i].sstamp.store(UINT64_MAX, memory_order_release);
		TMT[i].lastcstamp.store(0, memory_order_release);
		TMT[i].status.store(TransactionStatus::inFlight, memory_order_release);
	}
}

static void
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	//cout << diff << endl;
	//cout << CLOCK_PER_US * 1000000 << endl;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		sumTrans += FinishTransactions[i].num;
	}

	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}
static void *
manager_worker(void *arg)
{
	int *myid = (int *)arg;
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;
	
	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		ERR;
	}

	unsigned int expected, desired;
	do {
		expected = Running.load(std::memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));

	//spin-wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//-----
	
	//garbage collector
	while (Ending.load(memory_order_acquire) != THREAD_NUM - 1) {
		uint64_t mintxID = UINT64_MAX;
		for (unsigned int i = 1; i < THREAD_NUM; ++i) {
			mintxID = min(mintxID, ThtxID[i].num.load(memory_order_acquire));
		}
		if (mintxID == 0) continue;
		else {
			//mintxIDから到達不能なバージョンを削除する
			Version *verTmp, *delTarget;
			for (unsigned int i = 0; i < TUPLE_NUM; ++i) {

				verTmp = Table[i].latest.load(memory_order_acquire);
				if (verTmp->status.load(memory_order_acquire) != VersionStatus::committed) 
					verTmp = verTmp->committed_prev;
				// この時点で， verTmp はコミット済み最新バージョン

				uint64_t verCstamp = verTmp->cstamp.load(memory_order_acquire);
				while (mintxID < (verCstamp >> 1)) {
					verTmp = verTmp->committed_prev;
					if (verTmp == nullptr) break;
					verCstamp = verTmp->cstamp.load(memory_order_acquire);
				}
				if (verTmp == nullptr) continue;
				// verTmp は mintxID によって到達可能．
				
				// ssn commit protocol によってverTmp->commited_prev までアクセスされる．
				verTmp = verTmp->committed_prev;
				if (verTmp == nullptr) continue;
				
				// verTmp->prev からガベコレ可能
				delTarget = verTmp->prev;
				if (delTarget == nullptr) continue;

				verTmp->prev = nullptr;
				while (delTarget != nullptr) {
					verTmp = delTarget->prev;
					delete delTarget;
					delTarget = verTmp;
				}
				//-----
			}
		}
	}
	//-- garbage collector end---
	//
	return nullptr;
}

static void *
worker(void *arg)
{
	int *myid = (int *)arg;

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
	do {
		expected = Running.load();
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired));
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//-----
	
	//start work (transaction)
	if (*myid == 1) Bgn = rdtsc();

	try {
		Transaction trans(*myid, MAX_OPE);
		for (unsigned int i = PRO_NUM / (THREAD_NUM - 1) * (*myid - 1); i < PRO_NUM / (THREAD_NUM - 1) * (*myid); ++i) {
RETRY:
			//End judgment
			if (*myid == 1) {
				End = rdtsc();
				if (chkClkSpan(Bgn, End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
					do {
						expected = Ending.load(memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_weak(expected, desired, memory_order_acq_rel));
					Finish.store(true, std::memory_order_release);

					return nullptr;
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					do {
						expected = Ending.load(std::memory_order_acquire);
						desired = expected + 1;
						//printf("expected %d\n", expected);
					} while (!Ending.compare_exchange_weak(expected, desired, std::memory_order_acq_rel));

					return nullptr;
				}
			}
			//-----
			//transaction begin
			trans.tbegin();
			for (unsigned int j = 0; j < MAX_OPE; ++j) {
				unsigned int value_read;
				if (Pro[i][j].ope == Ope::READ) {
					value_read = trans.ssn_tread(Pro[i][j].key);
				} else if (Pro[i][j].ope == Ope::WRITE) {
					trans.ssn_twrite(Pro[i][j].key, Pro[i][j].val);
				} else {
					ERR;
				}

				if (trans.status == TransactionStatus::aborted) {
					trans.abort();
					goto RETRY;
				}
			}
			
			//trans.ssn_commit();
			trans.ssn_parallel_commit();
			if (trans.status == TransactionStatus::aborted) {
				trans.abort();
				goto RETRY;
			}

			
			if (i == (PRO_NUM / (THREAD_NUM - 1) * (*myid) - 1)) {
				i = PRO_NUM / (THREAD_NUM - 1) * (*myid - 1);
				//NNN;
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

extern void displayDB();
extern void displayPRO();
extern void displayAbortCounts();
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

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		thread[i] = threadCreate(i);
	}

	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		pthread_join(thread[i], nullptr);
	}

	//displayDB();

	prtRslt(Bgn, End);
	//displayAbortCounts();
	displayAbortRate();

	return 0;
}
