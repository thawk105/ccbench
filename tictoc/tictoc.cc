
#include <ctype.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>

#define GLOBAL_VALUE_DEFINE

#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"
#include "../include/zipf.hpp"

#include "include/common.hpp"
#include "include/result.hpp"
#include "include/transaction.hpp"

using namespace std;

extern bool chkClkSpan(uint64_t &start, uint64_t &stop, uint64_t threshold);
extern void displayDB();
extern void displayPRO();
extern void displayRtsudRate();
extern void makeDB();
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd);
extern void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd, FastZipf &zipf);
extern void setThreadAffinity(int myid);
extern void waitForReadyOfAllThread();

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
	if (argc != 9) {
    cout << "usage:./main TUPLE_NUM MAX_OPE THREAD_NUM RRATIO ZIPF_SKEW YCSB CLOCK_PER_US EXTIME" << endl << endl;

    cout << "example:./main 200 10 24 50 0 ON 2400 3" << endl << endl;

    cout << "TUPLE_NUM(int): total numbers of sets of key-value (1, 100), (2, 100)" << endl;
    cout << "MAX_OPE(int):    total numbers of operations" << endl;
    cout << "THREAD_NUM(int): total numbers of thread." << endl;
    cout << "RRATIO : read ratio [%%]" << endl;
    cout << "ZIPF_SKEW : zipf skew. 0 ~ 0.999..." << endl;
    cout << "YCSB : ON or OFF. switch makeProcedure function." << endl;
    cout << "CLOCK_PER_US: CPU_MHZ" << endl;
    cout << "EXTIME: execution time." << endl << endl;

	  cout << "Tuple " << sizeof(Tuple) << endl;
		cout << "uint64_t_64byte " << sizeof(uint64_t_64byte) << endl;
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
		cout << "rratio must be 0 ~ 10" << endl;
		ERR;
	}

  if (ZIPF_SKEW >= 1) {
    cout << "ZIPF_SKEW must be 0 ~ 0.999..." << endl;
    ERR;
  }

  if (argycsb == "ON")
    YCSB = true;
  else if (argycsb == "OFF")
    YCSB = false;
  else
    ERR;

  return;
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
	//printf("sysconf(_SC_NPROCESSORS_CONF) %d\n", sysconf(_SC_NPROCESSORS_CONF));
  waitForReadyOfAllThread();
	
	if (*myid == 0) rsobject.Bgn = rdtsc();

	try {
		//start work(transaction)
		for (;;) {
			if (*myid == 0) {
				rsobject.End = rdtsc();
				if (chkClkSpan(rsobject.Bgn, rsobject.End, EXTIME*1000*1000 * CLOCK_PER_US)) {
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

			//transaction begin
			//Read phase

      if (YCSB)
	  		makeProcedure(pro, rnd, zipf);
      else
        makeProcedure(pro, rnd);

			asm volatile ("" ::: "memory");
RETRY:
			trans.tbegin();
			for (unsigned int i = 0; i < MAX_OPE; ++i) {
				if (pro[i].ope == Ope::READ) {
					trans.tread(pro[i].key);
					if (trans.status == TransactionStatus::aborted) {
						trans.abort();
            ++rsobject.localAbortCounts;
						goto RETRY;
					}
				} else {
					trans.twrite(pro[i].key, pro[i].val);
				} 
			}
			
			//Validation phase
			if (trans.validationPhase()) {
				trans.writePhase();
        ++rsobject.localCommitCounts;
			} else {
				trans.abort();
        ++rsobject.localAbortCounts;
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
main(int argc, char *argv[]) 
{
  Result rsobject;
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

  rsobject.displayTPS();
  //rsobject.displayAbortRate();
	//displayRtsudRate();

	return 0;
}
