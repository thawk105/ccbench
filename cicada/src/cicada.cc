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
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM PRO_NUM READ_RATIO(0~1) \n\
             WAL GROUP_COMMIT CPU_MHZ IO_TIME_NS GROUP_COMMIT_TIMEOUT_US LOCK_RELEASE_METHOD EXTIME\n\
\n\
example:./main 1000000 20 15 10000 0.5 OFF OFF 2400 5 2 E 3\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of worker thread.\n\
PRO_NUM(int):    Initial total numbers of transactions.\n\
READ_RATIO(float): ratio of read in transaction.\n\
WAL: P or S or OFF.\n\
GROUP_COMMIT:	unsigned integer or OFF, i reccomend OFF or 3\n\
CPU_MHZ(float):	your cpuMHz. used by calculate time of yours 1clock.\n\
IO_TIME_NS: instead of exporting to disk, delay is inserted. the time(nano seconds).\n\
GROUP_COMMIT_TIMEOUT_US: Invocation condition of group commit by timeout(micro seconds).\n\
LOCK_RELEASE_METHOD: E or NE or N. Early lock release(tanabe original) or Normal Early Lock release or Normal lock release.\n\n");

		cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "Version " << sizeof(Version) << endl;
		cout << "TimeStamp " << sizeof(TimeStamp) << endl;
		cout << "pthread_mutex_t " << sizeof(pthread_mutex_t) << endl;

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
	
	string tmp = argv[6];
	if (tmp == "P")  {
		P_WAL = true;
		S_WAL = false;
	} else if (tmp == "S") {
		P_WAL = false;
		S_WAL = true;
	} else if (tmp == "OFF") {
		P_WAL = false;
		S_WAL = false;

		tmp = argv[7];
		if (tmp != "OFF") {
			printf("i don't implement below.\n\
P_WAL OFF, S_WAL OFF, GROUP_COMMIT number.\n\
usage: P_WAL or S_WAL is selected. \n\
P_WAL and S_WAL isn't selected, GROUP_COMMIT must be OFF. this isn't logging. performance is concurrency control only.\n\n");
			exit(0);
		}
	}
	else {
		printf("WAL(argv[6]) must be P or S or OFF\n");
		exit(0);
	}

	tmp = argv[7];
	if (tmp == "OFF") GROUP_COMMIT = 0;
	else if (chkInt(argv[7])) {
	   	GROUP_COMMIT = atoi(argv[7]);
	}
	else {
		printf("GROUP_COMMIT(argv[8]) must be unsigned integer or OFF\n");
		exit(0);
	}

	chkInt(argv[8]);
	CLOCK_PER_US = atof(argv[8]);
	if (CLOCK_PER_US < 100) {
		printf("CPU_MHZ is less than 100. are you really?\n");
		exit(0);
	}

	chkInt(argv[9]);
	IO_TIME_NS = atof(argv[9]);
	chkInt(argv[10]);
	GROUP_COMMIT_TIMEOUT_US = atoi(argv[10]);

	tmp = argv[11];
	if (tmp == "N") {
		NLR = true;
		ELR = false;
	}
	else if (tmp == "E") {
		NLR = false;
		ELR = true;
	}
	else {
		printf("LockRelease(argv[11]) must be E or N\n");
		exit(0);
	}

	chkInt(argv[12]);
	EXTIME = atoi(argv[12]);
	
	if (THREAD_NUM > PRO_NUM) {
		printf("THREAD_NUM must be smaller than PRO_NUM\n");
		exit(0);
	}
	try {

		ThreadWtsArray = new TimeStamp*[THREAD_NUM];
		//allo = posix_memalign((void**)ThreadWtsArray, 64, THREAD_NUM * sizeof(TimeStamp*));
		ThreadRtsArray = new TimeStamp*[THREAD_NUM];
		if (posix_memalign((void**)&ThreadRtsArrayForGroup, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&ThreadWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadRts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadFlushedWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_INDEX, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_COUNTER, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCFlag, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		
		SLogSet = new Version*[(MAX_OPE) * (GROUP_COMMIT)];	//実装の簡単のため、どちらもメモリを確保してしまう。
		PLogSet = new Version**[THREAD_NUM];

		for (unsigned int i = 0; i < THREAD_NUM; ++i) {
			PLogSet[i] = new Version*[(MAX_OPE) * (GROUP_COMMIT)];
		}
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		AbortCounts[i].num = 0;
		FinishTransactions[i].num = 0;
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
	/*long usec;
	double sec;

	usec = (end.tv_sec - bgn.tv_sec) * 1000 * 1000 + (end.tv_usec - bgn.tv_usec);
	sec = (double) usec / 1000.0 / 1000.0;

	double result = (double)sumTrans / sec;
	//cout << (int)sumTrans << endl;
	//cout << (int)sec << endl;
	cout << (int)result << endl;
	fflush(stdout);
	*/

	uint64_t diff = end - bgn;
	uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;

	int sumTrans = 0;
	for (unsigned int i = 0; i < THREAD_NUM; ++i) {
		//cout << "FinishTransactions[" << i << "]: " << FinishTransactions[i] << endl;
		sumTrans += FinishTransactions[i].num;
	}
	
	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
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

	//実装上Table[TUPLE_NUM - 1]が一番最後に作られているので、それを参照できたら良い
	Version *verTmp = Table[TUPLE_NUM - 1].latest.load();
	MinRts = verTmp->wts + 1;
	for (unsigned int i = 1; i < THREAD_NUM; ++i) {
		ThreadRts[i].ts = MinRts;
		if (GROUP_COMMIT)
			ThreadRtsArrayForGroup[i].num = MinRts;
	}

	unsigned int expected, desired;
	do {
		expected = Running.load(memory_order_acquire);
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired, memory_order_acq_rel));

	while (Running.load(memory_order_acquire) != THREAD_NUM) {}

	// leader work
	while (Ending.load(memory_order_acquire) != THREAD_NUM - 1) {
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

static void *
worker(void *arg)
{
	int *myid = (int *)arg;

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
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//----------
	
	//start work(transaction)
	if (*myid == 1) Bgn = rdtsc();

	try {
		Transaction trans(&ThreadRts[*myid], &ThreadWts[*myid], *myid);
		for (unsigned int i = PRO_NUM / (THREAD_NUM-1) * (*myid - 1); i < PRO_NUM / (THREAD_NUM-1) * (*myid); ++i) {
RETRY:
			if (*myid == 1) {
				End = rdtsc();
				if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
					do {
						expected = Ending.load(memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_strong(expected, desired, memory_order_acq_rel));
					Finish.store(true, std::memory_order_release);
					return nullptr;
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					do {
						expected = Ending.load(memory_order_acquire);
						desired = expected + 1;
					} while (!Ending.compare_exchange_strong(expected, desired, memory_order_acq_rel));
					return nullptr;
				}
			}

			trans.tbegin(i);

			//Read phase
			//Search versions
			for (unsigned int j = 0; j < MAX_OPE; ++j) {
				switch(Pro[i][j].ope) {
					case(Ope::READ):
						trans.tread(Pro[i][j].key);
						break;
					case(Ope::WRITE):
						trans.twrite(Pro[i][j].key, Pro[i][j].val);
						
						//early abort
						if (trans.status == TransactionStatus::abort) {
							trans.earlyAbort();
							goto RETRY;
						}
						break;
					default:
						break;
				}
			}

			//read onlyトランザクションはread setを集めず、validationもしない。
			//write phaseはログを取り仮バージョンのコミットを行う．これをスキップできる．
			if (trans.ronly) {
				if (i == PRO_NUM / (THREAD_NUM - 1) * (*myid) - 1) {
					//NNN;
					i = PRO_NUM / (THREAD_NUM - 1) * (*myid - 1);
				}
				FinishTransactions[*myid].num++;
				continue;
			}

			//Validation phase
			if (!trans.validation()) {
				trans.abort();
				goto RETRY;
			}

			//Write phase
			trans.writePhase();

			//Maintenance
			//Schedule garbage collection
			//Declare quiescent state
			//Collect garbage created by prior transactions
			trans.mainte(i);

			if (i == PRO_NUM / (THREAD_NUM - 1) * (*myid) - 1) {
				i = PRO_NUM / (THREAD_NUM - 1) * (*myid - 1);
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
extern void makeProcedure();
extern void makeDB();
extern void mutexInit();

int 
main(int argc, char *argv[]) {

	chkArg(argc, argv);

	mutexInit();
	makeDB();
	makeProcedure();

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
	
	prtRslt(Bgn, End);

	return 0;
}
