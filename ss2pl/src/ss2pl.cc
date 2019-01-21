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
#include "include/result.hpp"
#include "include/transaction.hpp"
#include "include/tsc.hpp"
#include "include/zipf.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

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
	if (argc != 9) {
		cout << "usage: ./ss2pl.exe TUPLE_NUM MAX_OPE THREAD_NUM RRATIO ZIPF_SKEW YCSB CPU_MHZ EXTIME" << endl << endl;
		cout << "example: ./ss2pl.exe 200 10 24 50 0 ON 2400 3" << endl << endl;
		cout << "TUPLE_NUM(int): total numbers of sets of key-value" << endl;
		cout << "MAX_OPE(int): total numbers of operations" << endl;
		cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
		cout << "RRATIO : read ratio [%%]" << endl;
		cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;

    cout << "YCSB : ON or OFF. switch makeProcedure function." << endl;
		cout << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock" << endl;
		cout << "EXTIME: execution time [sec]" << endl << endl;

		cout << "Tuple size " << sizeof(Tuple) << endl;
		cout << "std::mutex size " << sizeof(mutex) << endl;
		cout << "RWLock size " << sizeof(RWLock) << endl;
		exit(0);
	}

	chkInt(argv[1]);
	chkInt(argv[2]);
	chkInt(argv[3]);
	chkInt(argv[4]);
	chkInt(argv[7]);
	chkInt(argv[8]);

	TUPLE_NUM = atoi(argv[1]);
	MAX_OPE = atoi(argv[2]);
	THREAD_NUM = atoi(argv[3]);
	RRATIO = atoi(argv[4]);
  ZIPF_SKEW = atof(argv[5]);
  string argycsb = argv[6];
	CLOCK_PER_US = atof(argv[7]);
	EXTIME = atoi(argv[8]);

	if (RRATIO > 100) {
		ERR;
	}

	if (CLOCK_PER_US < 100) {
		cout << "CPU_MHZ is less than 100. are your really?" << endl;
		ERR;
	}
}

static void *
worker(void *arg)
{

	const int *myid = (int *)arg;
	Xoroshiro128Plus rnd;
	rnd.init();
	Procedure pro[MAX_OPE];
	Transaction trans(*myid);
  Result rsobject;
  FastZipf zipf(&rnd, ZIPF_SKEW, TUPLE_NUM);

  setThreadAffinity(*myid);
	//printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
	//printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();
	
	if (*myid == 0) rsobject.Bgn = rdtsc();

	try {
		//start work (transaction)
		for (;;) {
			//End judgment
			if (*myid == 0) {
				rsobject.End = rdtsc();
				if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME * 1000 * 1000 * CLOCK_PER_US)) {
					rsobject.Finish.store(true, std::memory_order_release);
          rsobject.sumUpCommitCounts();
          rsobject.sumUpAbortCounts();
					return nullptr;
				}
			} else {
				if (rsobject.Finish.load(std::memory_order_acquire)) {
          rsobject.sumUpCommitCounts();
          rsobject.sumUpAbortCounts();
					return nullptr;
				}
			}
			//-----
		  if (YCSB)	
			  makeProcedure(pro, rnd, zipf);
      else
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
          ++rsobject.localAbortCounts;
					goto RETRY;
				}
			}

			//commit - write phase
			trans.commit();
      ++rsobject.localCommitCounts;
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
main(const int argc, const char *argv[])
{
  Result rsobject;
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

  rsobject.displayTPS();
	rsobject.displayAbortRate();

	return 0;
}
