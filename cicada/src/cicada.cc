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
	if (argc != 15) {
		printf("usage:./main TUPLE_NUM MAX_OPE THREAD_NUM PRO_NUM READ_RATIO(0~1) SPIN_WAIT_TIMEOUT_US \n\
             WAL GROUP_COMMIT CPU_MHZ IO_TIME_NS GROUP_COMMIT_TIMEOUT_US GARBAGE_COLLECTION_INTERVAL_US LOCK_RELEASE_METHOD EXTIME\n\
\n\
example:./main 1000000 20 15 10000 0.5 2 OFF OFF 2400 5 2 0 E 3\n\
\n\
TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)\n\
MAX_OPE(int):    total numbers of operations\n\
THREAD_NUM(int): total numbers of worker thread.\n\
PRO_NUM(int):    Initial total numbers of transactions.\n\
READ_RATIO(float): ratio of read in transaction.\n\
SPIN_WAIT_TIMEOUT_US(int):   limit of waiting time(µs) to commit pending version by other.\n\
WAL: P or S or OFF.\n\
GROUP_COMMIT:	unsigned integer or OFF, i reccomend OFF or 3\n\
CPU_MHZ(float):	your cpuMHz. used by calculate time of yours 1clock.\n\
IO_TIME_NS: instead of exporting to disk, delay is inserted. the time(nano seconds).\n\
GROUP_COMMIT_TIMEOUT_US: Invocation condition of group commit by timeout(micro seconds).\n\
GARBAGE_COLLECTION_INTERVAL_US: Interval for calling garbage collection(micro seconds). i reccomend 0. Setting 0 is executing for each transaction.\n\
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
	chkInt(argv[6]);
	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	PRO_NUM = atoi(argv[4]);
	READ_RATIO = atof(argv[5]);
	SPIN_WAIT_TIMEOUT_US = atoi(argv[6]);
	
	string tmp = argv[7];
	if (tmp == "P")  {
		P_WAL = true;
		S_WAL = false;
	} else if (tmp == "S") {
		P_WAL = false;
		S_WAL = true;
	} else if (tmp == "OFF") {
		P_WAL = false;
		S_WAL = false;

		tmp = argv[8];
		if (tmp != "OFF") {
			printf("i don't implement below.\n\
P_WAL OFF, S_WAL OFF, GROUP_COMMIT number.\n\
usage: P_WAL or S_WAL is selected. \n\
P_WAL and S_WAL isn't selected, GROUP_COMMIT must be OFF. this isn't logging. performance is concurrency control only.\n\n");
			exit(0);
		}
	}
	else {
		printf("WAL(argv[7]) must be P or S or OFF\n");
		exit(0);
	}

	tmp = argv[8];
	if (tmp == "OFF") GROUP_COMMIT = 0;
	else if (chkInt(argv[8])) {
	   	GROUP_COMMIT = atoi(argv[8]);
	}
	else {
		printf("GROUP_COMMIT(argv[8]) must be unsigned integer or OFF\n");
		exit(0);
	}

	chkInt(argv[9]);
	CLOCK_PER_US = atof(argv[9]);
	if (CLOCK_PER_US < 100) {
		printf("CPU_MHZ is less than 100. are you really?\n");
		exit(0);
	}

	chkInt(argv[10]);
	IO_TIME_NS = atof(argv[10]);
	chkInt(argv[11]);
	GROUP_COMMIT_TIMEOUT_US = atoi(argv[11]);
	chkInt(argv[12]);
	GARBAGE_COLLECTION_INTERVAL_US = atoi(argv[12]);
	if (GARBAGE_COLLECTION_INTERVAL_US < 0) {
		printf("GARBAGE_COLLECTION_INTERVAL_US must be positive number.( > 0)\n");
		exit(0);
	}

	//ELR	tanabe		early lock release
	//NLR 				normal lock release
	//NER	normal 		early lock release
	tmp = argv[13];
	if (tmp == "N") {
		NLR = true;
		ELR = false;
	}
	else if (tmp == "E") {
		NLR = false;
		ELR = true;
	}
	else {
		printf("LockRelease(argv[13]) must be E or N\n");
		exit(0);
	}

	chkInt(argv[14]);
	EXTIME = atoi(argv[14]);
	
	if (THREAD_NUM > PRO_NUM) {
		printf("THREAD_NUM must be smaller than PRO_NUM\n");
		exit(0);
	}
	try {
		//cout << "timestamp size " << sizeof(TimeStamp) << endl;	// sizeof(TimeStamp) 	was 24
		//cout << "uint64_t size " << sizeof(uint64_t) << endl;		// sizeof(uint64_t) 	was 8
		//cout << "int size " << sizeof(int) << endl;				// sizeof(int) 			was 4
		//cout << "timeval size " << sizeof(timeval) << endl;		// sizeof(timeval) 		was 16

		ThreadWtsArray = new TimeStamp*[THREAD_NUM];
		//allo = posix_memalign((void**)ThreadWtsArray, 64, THREAD_NUM * sizeof(TimeStamp*));
		//seg fault 未解明
		ThreadRtsArray = new TimeStamp*[THREAD_NUM];
		if (posix_memalign((void**)&ThreadRtsArrayForGroup, 64, THREAD_NUM * sizeof(uint64_t)) != 0) ERR;
		if (posix_memalign((void**)&ThreadWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadRts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&ThreadFlushedWts, 64, THREAD_NUM * sizeof(TimeStamp)) != 0) ERR;
		if (posix_memalign((void**)&FirstAllocateTimeStamp, 64, THREAD_NUM * sizeof(std::atomic<bool>)) != 0) ERR;
		if (posix_memalign((void**)&AbortCounts, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_INDEX, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GROUP_COMMIT_COUNTER, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Start, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&Stop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCommitStart, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
	    if (posix_memalign((void**)&GCommitStop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCollectionStart, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&GCollectionStop, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		if (posix_memalign((void**)&FinishTransactions, 64, THREAD_NUM * sizeof(uint64_t_64byte)) != 0) ERR;
		
		SLogSet = new Version*[(MAX_OPE) * (GROUP_COMMIT)];	//実装の簡単のため、どちらもメモリを確保してしまう。
		PLogSet = new Version**[THREAD_NUM];

		MessageQueue = new std::queue<Message>[THREAD_NUM];	//コミット通知用
		for (int i = 0; i < THREAD_NUM; i++) {
			PLogSet[i] = new Version*[(MAX_OPE) * (GROUP_COMMIT)];
		}
	} catch (bad_alloc) {
		ERR;
	}
	//init
	for (int i = 0; i < THREAD_NUM; i++) {
		FirstAllocateTimeStamp[i].store(false, memory_order_release);
		AbortCounts[i].num = 0;
		GROUP_COMMIT_INDEX[i].num = 0;
		GROUP_COMMIT_COUNTER[i].num = 0;
		FinishTransactions[i].num = 0;
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
	for (int i = 0; i < THREAD_NUM; i++) {
		//cout << "FinishTransactions[" << i << "]: " << FinishTransactions[i] << endl;
		sumTrans += FinishTransactions[i].num;
	}
	
	uint64_t result = (double)sumTrans / (double)sec;
	cout << (int)result << endl;
}

extern bool chkSpan(struct timeval &start, struct timeval &stop, long threshold);
extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);

pid_t gettid(void) 
{
	return syscall(SYS_gettid);
}

static void *
worker(void *arg)
{
	int *myid = (int *)arg;

	//----------
	pid_t pid;
	cpu_set_t cpu_set;
	int result;
	Version *verTmp;

	pid = gettid();
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
	
	bool firstFlag = true;
	TimeStamp& thwts = ThreadWts[*myid];
	TimeStamp& thrts = ThreadRts[*myid];
	ThreadRtsArray[*myid] = &thrts;
	ThreadWtsArray[*myid] = &thwts;

	
	//実装上HashTable[0]が一番最後に作られているので、それを参照できたら良い
	verTmp = HashTable[0].latest.load();
	MinRts = verTmp->wts + 1;
	
	//前処理
	GCollectionStart[*myid].num = rdtsc();

	//----------
	//wait for all threads start. CAS.
	int expected, desired;
	do {
		expected = Running.load();
		desired = expected + 1;
	} while (!Running.compare_exchange_weak(expected, desired));
	
	//spin wait
	while (Running.load(std::memory_order_acquire) != THREAD_NUM) {}
	//----------
	
	//start work(transaction)
	if (*myid == 0) Bgn = rdtsc();


	try {
		uint64_t localFinishTransactions = 0;
		uint64_t localAbortCounts = 0;

		for (int i = PRO_NUM / THREAD_NUM * (*myid); i < PRO_NUM / THREAD_NUM * (*myid + 1); i++) {
RETRY:
			if (*myid == 0) {
				if (FinishTransactions[*myid].num % 1000 == 0) {
					End = rdtsc();
					if (chkClkSpan(Bgn, End, EXTIME*1000*1000 * CLOCK_PER_US)) {
						Finish.store(true, std::memory_order_release);
						FinishTransactions[*myid].num = localFinishTransactions;
						AbortCounts[*myid].num = localAbortCounts;
						return NULL;
					}
				}
			} else {
				if (Finish.load(std::memory_order_acquire)) {
					FinishTransactions[*myid].num = localFinishTransactions;
						AbortCounts[*myid].num = localAbortCounts;
					return NULL;
				}
			}

			Transaction trans;
			trans.tbegin(thrts, thwts, firstFlag, *myid, i);

			//test
			//random_device rnd;
			//----

			//Read phase
			//Search versions
			for (int j = 0; j < MAX_OPE; j++) {
				int value_read;

				//--test--
				//if ((rnd() % 100) < (READ_RATIO * 100))
				//	Pro[i][j].ope = Ope::READ;
				//else
				//	Pro[i][j].ope = Ope::WRITE;
				////--------
				
				switch(Pro[i][j].ope) {
					case(Ope::READ):
						value_read = trans.tread(Pro[i][j].key);
						//printf("read(%d) = %d\n", Pro[i][j].key, value_read);
						break;
					case(Ope::WRITE):
						//test
						//Pro[i][j].key = rnd() % TUPLE_NUM + 1;
						//Pro[i][j].val = rnd() % (TUPLE_NUM*10);
						//----
						trans.twrite(Pro[i][j].key, Pro[i][j].val);
						
						//early abort
						if (trans.status == TransactionStatus::abort) {
							trans.earlyAbort();
							localAbortCounts++;
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
				if (i == (PRO_NUM / THREAD_NUM * (*myid + 1)) - 1) {
					//NNN;
					i = PRO_NUM / THREAD_NUM * (*myid);
				}
				localFinishTransactions++;
				//FinishTransactions[*myid].num++;
				continue;
			}

			//Validation phase
			if (!trans.validation()) {
				trans.abort();
				localAbortCounts++;
				goto RETRY;
			}

			//Write phase
			trans.writePhase();

			//クライアントへの通知
			//trans.noticeToClients();

			//Maintenance
			//Schedule garbage collection
			//Declare quiescent state
			//Collect garbage created by prior transactions
			trans.mainte(i);

			if (i == (PRO_NUM / THREAD_NUM * (*myid + 1)) - 1) {
				//NNN;
				i = PRO_NUM / THREAD_NUM * (*myid);
			}
			localFinishTransactions++;
			//FinishTransactions[*myid].num++;
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

	//test
	/*if (pthread_mutex_lock(&HashTable[0].lock)) ERR;
	cout << "success!" << endl;
	if (pthread_mutex_unlock(&HashTable[0].lock)) ERR;
	printf("main(int argc, char *argv[]): sizeof(Version) = %d\n", sizeof(Version));*/
	
	//displayDB();
	//displayPRO();

	pthread_t thread[THREAD_NUM];

	for (int i = 0; i < THREAD_NUM; i++) {
		thread[i] = threadCreate(i);
	}

	for (int i = 0; i < THREAD_NUM; i++) {
		pthread_join(thread[i], NULL);
	}

	//displayDB();
	//displayMinRts();
	//displayMinWts();
	//displayThreadWtsArray();
	//displayThreadRtsArray();
	displayAbortRate();
	
	//prtRslt(Bgn, End);

	return 0;
}
