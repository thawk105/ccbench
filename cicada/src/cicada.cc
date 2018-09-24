#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctype.h>
#include <pthread.h>
#include <random>
#include <string.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#define GLOBAL_VALUE_DEFINE
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/int64byte.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"
#include "include/transaction.hpp"

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
	if (argc != 13) {
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM WORKLOAD(1~5) \n\
             WAL GROUP_COMMIT CPU_MHZ IO_TIME_NS GROUP_COMMIT_TIMEOUT_US LOCK_RELEASE_METHOD EXTIME UNSTABLE_WORKLOAD\n\
\n\
example:./main 1000000 20 15 3 OFF OFF 2400 5 2 E 3 0\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of worker thread.\n\
WORKLOAD:\n\
0. read only (read 100%%)\n\
1. read intensive (read 80%%)\n\
2. read write even (read 50%%)\n\
3. write intensive (write 80%%)\n\
4. write only (write 100%%)\n\
WAL: P or S or OFF.\n\
GROUP_COMMIT:	unsigned integer or OFF, i reccomend OFF or 3\n\
CPU_MHZ(float):	your cpuMHz. used by calculate time of yours 1clock.\n\
IO_TIME_NS: instead of exporting to disk, delay is inserted. the time(nano seconds).\n\
GROUP_COMMIT_TIMEOUT_US: Invocation condition of group commit by timeout(micro seconds).\n\
LOCK_RELEASE_METHOD: E or NE or N. Early lock release(tanabe original) or Normal Early Lock release or Normal lock release.\n\
UNSTABLE_WORKLOAD: \n\
0. stable workload\n\
1. switch read-mostly and write-mostly every one second\n\n");

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "Version " << sizeof(Version) << endl;
		cout << "TimeStamp " << sizeof(TimeStamp) << endl;
		cout << "Procedure" << sizeof(Procedure) << endl;
		exit(0);
	}
	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	WORKLOAD = atoi(argv[4]);
	if (WORKLOAD > 4) {
		cout << "workload must be 0 ~ 4" << endl;
	}
	
	string tmp = argv[5];
	if (tmp == "P")  {
		P_WAL = true;
		S_WAL = false;
	} else if (tmp == "S") {
		P_WAL = false;
		S_WAL = true;
	} else if (tmp == "OFF") {
		P_WAL = false;
		S_WAL = false;

		tmp = argv[6];
		if (tmp != "OFF") {
			printf("i don't implement below.\n\
P_WAL OFF, S_WAL OFF, GROUP_COMMIT number.\n\
usage: P_WAL or S_WAL is selected. \n\
P_WAL and S_WAL isn't selected, GROUP_COMMIT must be OFF. this isn't logging. performance is concurrency control only.\n\n");
			exit(0);
		}
	}
	else {
		printf("WAL(argv[5]) must be P or S or OFF\n");
		exit(0);
	}

	tmp = argv[6];
	if (tmp == "OFF") GROUP_COMMIT = 0;
	else if (chkInt(argv[6])) {
	   	GROUP_COMMIT = atoi(argv[6]);
	}
	else {
		printf("GROUP_COMMIT(argv[6]) must be unsigned integer or OFF\n");
		exit(0);
	}

	chkInt(argv[7]);
	CLOCK_PER_US = atof(argv[7]);
	if (CLOCK_PER_US < 100) {
		printf("CPU_MHZ is less than 100. are you really?\n");
		exit(0);
	}

	chkInt(argv[8]);
	IO_TIME_NS = atof(argv[8]);
	chkInt(argv[9]);
	GROUP_COMMIT_TIMEOUT_US = atoi(argv[9]);

	tmp = argv[10];
	if (tmp == "N") {
		NLR = true;
		ELR = false;
	}
	else if (tmp == "E") {
		NLR = false;
		ELR = true;
	}
	else {
		printf("LockRelease(argv[10]) must be E or N\n");
		exit(0);
	}

	chkInt(argv[11]);
	EXTIME = atoi(argv[11]);
	UNSTABLE_WORKLOAD = atoi(argv[12]);
	if (UNSTABLE_WORKLOAD > 1) {
		cout << "UNSTABLE_WORKLOAD must be 0 or 1" << endl;
		ERR;
	}
	
	try {

		ThreadWtsArray = new TimeStamp*[THREAD_NUM];
		//allo = posix_memalign((void**)ThreadWtsArray, 64, THREAD_NUM * sizeof(TimeStamp*));
		ThreadRtsArray = new TimeStamp*[THREAD_NUM];
		if (posix_memalign((void**)&ThreadRtsArrayForGroup, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&ThreadWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadRts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadFlushedWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_INDEX, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_COUNTER, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCommitStart, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCommitStop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCFlag, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		
		SLogSet = new Version*[(MAX_OPE) * (GROUP_COMMIT)];	//実装の簡単のため、どちらもメモリを確保してしまう。
		PLogSet = new Version**[THREAD_NUM];
		FinishTransactions = new uint64_t[THREAD_NUM];
		AbortCounts = new uint64_t[THREAD_NUM];
		UnstaFinishTransactions = new uint64_t[EXTIME * 10 + 1]; // unstable workload change by 0.1 sec

		for (unsigned int i = 0; i < THREAD_NUM; ++i) {
			PLogSet[i] = new Version*[(MAX_OPE) * (GROUP_COMMIT)];
		}
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		FinishTransactions[i] = 0;
		AbortCounts[i] = 0;
		UnstaFinishTransactions[i] = 0;
		GCFlag[i].num = 0;
		GROUP_COMMIT_INDEX[i].num = 0;
		GROUP_COMMIT_COUNTER[i].num = 0;
		ThreadRtsArray[i] = &ThreadRts[i];
		ThreadWtsArray[i] = &ThreadWts[i];
		ThreadRtsArrayForGroup[i].num = 0;
		ThreadRts[i].ts = 0;
		ThreadWts[i].ts = 0;
	}
}

static void 
prtRslt(uint64_t &bgn, uint64_t &end)
{
	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		//cout << "FinishTransactions[" << i << "]: " << FinishTransactions[i] << endl;
		sumTrans += FinishTransactions[i];
	}
	
	if (UNSTABLE_WORKLOAD == 0) {
		uint64_t result = (double)sumTrans / (double)sec;
		cout << (int)result << endl;
	} else {
		if (UNSTABLE_WORKLOAD) {
			for (unsigned int i = 1; i < EXTIME * 10 + 1; ++i) {
				cout << 0.1 * i << " " << UnstaFinishTransactions[i] << endl;
			}
		}
	}
}

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

static void *
maneger_worker(void *arg)
{
	int *myid = (int *)arg;
	pid_t pid = syscall(SYS_gettid);
	cpu_set_t cpu_set;

	CPU_ZERO(&cpu_set);
	CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		ERR;
	}
	//printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());

	MinWts.store(InitialWts + 2, memory_order_release);

	unsigned int expected, desired;
	do {
		expected = Running.load(memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, memory_order_acq_rel));

	while (Running.load(memory_order_acquire) != THREAD_NUM) {}
	while (FirstAllocateTimestamp.load(memory_order_acquire) != THREAD_NUM - 1) {}

	uint64_t finish_time = EXTIME * 1000 * 1000 * CLOCK_PER_US;
	

	Bgn = rdtsc();
	// leader work
	for(;;) {
		if (UNSTABLE_WORKLOAD == 0) {
			End = rdtsc();
			if (chkClkSpan(Bgn, End, finish_time)) {
				Finish.store(true, std::memory_order_release);
				return nullptr;
			}
		} else if (UNSTABLE_WORKLOAD == 1) {
			if (Ending.load(memory_order_acquire) == THREAD_NUM - 1) return nullptr;
		}

		bool gc_update = true;
		for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		//check all thread's flag raising
			if (GCFlag[i].num == 0) {
				gc_update = false;
				break;
			}
		}
		if (gc_update) {
			uint64_t minw = ThreadWtsArray[1]->ts;
			uint64_t minr;
			if (GROUP_COMMIT == 0) {
				minr = ThreadRtsArray[1]->ts;
			}
			else {
				minr = ThreadRtsArrayForGroup[1].num;
			}

			for (unsigned int i = 1; i < THREAD_NUM; ++i) {
				uint64_t tmp = ThreadWtsArray[i]->ts;
				if (minw > tmp) minw = tmp;
				if (GROUP_COMMIT == 0) {
					tmp = ThreadRtsArray[i]->ts;
					if (minr > tmp) minr = tmp;
				}
				else {
					tmp = ThreadRtsArrayForGroup[i].num;
					if (minr > tmp) minr = tmp;
				}
			}
			MinWts.store(minw, memory_order_release);
			MinRts.store(minr, memory_order_release);

			// downgrade gc flag
			for (unsigned int i = 1; i < THREAD_NUM; ++i) {
				GCFlag[i].num = 0;
			}
		}
	}

	return nullptr;
}

extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, unsigned int localWL);

static void *
worker(void *arg)
{
	int *myid = (int *)arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	uint64_t localBgn, localEnd;
	unsigned int local_unsta_index(1), local_un_wl(1);

	//----------
	pid_t pid;
	cpu_set_t cpu_set;

	pid = syscall(SYS_gettid);
	CPU_ZERO(&cpu_set);
	//int core_num = *myid % sysconf(_SC_NPROCESSORS_CONF) * 2;
	//if (core_num > 23) core_num -= 23;	//	chris numa 対策
	int core_num = *myid % sysconf(_SC_NPROCESSORS_CONF);
	CPU_SET(core_num, &cpu_set);

	if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
		printf("thread affinity setting is error.\n");
		exit(1);
	}
	//check-test
	//printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
	//printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
	//----------
	
	//wait for all threads start. CAS.
	unsigned int expected, desired;
	do {
		expected = Running.load();
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired));
	
	uint64_t totalFinishTransactions(0);
	uint64_t localUnstaTrans[EXTIME * 10 + 1] = {};
	uint64_t totalAbortCounts(0);
	Transaction trans(&ThreadRts[*myid], &ThreadWts[*myid], *myid);

	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//----------
	
	localBgn = rdtsc();
	try {
		//start work(transaction)
		for (;;) {
			if (UNSTABLE_WORKLOAD == 0) {
				if (Finish.load(std::memory_order_acquire)) {
					CtrLock.w_lock();
					FinishTransactions[*myid] = totalFinishTransactions;
					AbortCounts[*myid] = totalAbortCounts;
					for (unsigned int i = 1; i < EXTIME * 10 + 1; ++i)
						UnstaFinishTransactions[i] = localUnstaTrans[i];
					CtrLock.w_unlock();
					return nullptr;
				}
			} else {
				localEnd = rdtsc();
				if (chkClkSpan(localBgn, localEnd, CLOCK_PER_US * 1000 * 100)) {
					// elapsed 0.1 sec
					if (local_unsta_index == EXTIME * 10) {
						do {
							expected = Ending;
							desired = expected + 1;
						} while (!Ending.compare_exchange_weak(expected, desired, memory_order_acq_rel));
						CtrLock.w_lock();
						FinishTransactions[*myid] = totalFinishTransactions;
						AbortCounts[*myid] = totalAbortCounts;
						for (unsigned int i = 1; i < EXTIME * 10 + 1; ++i)
							UnstaFinishTransactions[i] = localUnstaTrans[i];
						CtrLock.w_unlock();
						return nullptr;
					}

					//change workload
					if (local_un_wl == 1)
						local_un_wl = 3;
					else 
						local_un_wl = 1;

					local_unsta_index++;
					localBgn = localEnd;
				}
			}

			if (UNSTABLE_WORKLOAD == 0) {
				makeProcedure(pro, rnd, WORKLOAD);
			} else {
				makeProcedure(pro, rnd, local_un_wl);
			}
			asm volatile ("" ::: "memory");
RETRY:
			trans.tbegin(pro[0].ronly);

			//Read phase
			//Search versions
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.tread(pro[i].key);
				} else {
					trans.twrite(pro[i].key, pro[i].val);
				}

				if (trans.status == TransactionStatus::abort) {
					trans.earlyAbort();
					totalAbortCounts++;
					goto RETRY;
				}
			}

			//read onlyトランザクションはread setを集めず、validationもしない。
			//write phaseはログを取り仮バージョンのコミットを行う．これをスキップできる．
			if (trans.ronly) {
				totalFinishTransactions++;
				if (UNSTABLE_WORKLOAD) localUnstaTrans[local_unsta_index]++;
				continue;
			}

			//Validation phase
			if (!trans.validation()) {
				trans.abort();
				totalAbortCounts++;
				goto RETRY;
			}

			//Write phase
			trans.writePhase();
			totalFinishTransactions++;
			if (UNSTABLE_WORKLOAD) localUnstaTrans[local_unsta_index]++;

			//Maintenance
			//Schedule garbage collection
			//Declare quiescent state
			//Collect garbage created by prior transactions
			trans.mainte();
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

	if (id == 0) {
		if (pthread_create(&t, nullptr, maneger_worker, (void *)myid)) ERR;
		return t;
	}
	else {
		if (pthread_create(&t, nullptr, worker, (void *)myid)) ERR;
		return t;
	}
}

extern void displayMinRts();
extern void displayMinWts();
extern void displayThreadWtsArray();
extern void displayThreadRtsArray();
extern void displayDB();
extern void displayPRO();
extern void displayAbortRate();
extern void displayFinishTransactions();
extern void displayAbortCounts();
extern void displayTransactionRange();
extern void makeDB();

int 
main(int argc, char *argv[]) {

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

	//displayDB();
	//displayMinRts();
	//displayMinWts();
	//displayThreadWtsArray();
	//displayThreadRtsArray();
	//displayAbortRate();
	//displayFinishTransactions();
	//displayAbortCounts();
	//displayTransactionRange();
	
	prtRslt(Bgn, End);

	return 0;
}
