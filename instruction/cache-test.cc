#include <ctype.h>  //isdigit,
#include <pthread.h>
#include <string.h>       //strlen,
#include <sys/syscall.h>  //syscall(SYS_gettid),
#include <sys/types.h>    //syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>     //syscall(SYS_gettid),
#include <xmmintrin.h>  // _mm_pause

#include <iostream>
#include <string>  //string

#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/tsc.hh"

#define GLOBAL_VALUE_DEFINE

#include "include/common.hh"

using namespace std;

extern bool chkClkSpan(const uint64_t start, const uint64_t stop,
                       const uint64_t threshold);

static bool chkInt(const char *arg) {
  for (unsigned int i = 0; i < strlen(arg); ++i) {
    if (!isdigit(arg[i])) {
      cout << std::string(arg) << " is not a number." << endl;
      exit(0);
    }
  }
  return true;
}

static void chkArg(const int argc, const char *argv[]) {
  if (argc != 4) {
    cout << "usage: ./a.out THREAD_NUM CPU_MHZ EXTIME" << endl;
    cout << "example: ./a.out 24 2400 3" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout
            << "CPU_MHZ(float): your cpuMHz. used by calculate time of yorus 1clock"
            << endl;
    cout << "EXTIME: execution time [sec]" << endl;

    exit(0);
  }

  chkInt(argv[1]);
  chkInt(argv[2]);
  chkInt(argv[3]);

  THREAD_NUM = atoi(argv[1]);
  // if (THREAD_NUM < 2) {
  //  cout << "THREAD_NUM must be larger than 1" << endl;
  //  ERR;
  //}

  CLOCK_PER_US = atof(argv[2]);
  if (CLOCK_PER_US < 100) {
    cout << "CPU_MHZ is less than 100. are your really?" << endl;
    ERR;
  }

  EXTIME = atoi(argv[3]);
}

// static void
// prtRslt(uint64_t &bgn, uint64_t &end)
//{
//   現在不使用
//   now, don't be cared.
//
//  uint64_t diff = end - bgn;
//  cout << diff << endl;
//  cout << CLOCK_PER_US * 1000000 << endl;
//  uint64_t sec = diff / CLOCK_PER_US / 1000 / 1000;
//
//  uint64_t result = (double)sum / (double)sec;
//  cout << (int)result << endl;
//}

static void *manager_worker(void *arg) {
  int *myid = (int *) arg;
  pid_t pid = syscall(SYS_gettid);
  cpu_set_t cpu_set;

  CPU_ZERO(&cpu_set);
  CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) ERR;

  unsigned int expected, desired;
  expected = Running.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + 1;
    if (Running.compare_exchange_weak(expected, desired,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire))
      break;
  }

  // spin-wait
  while (Running.load(std::memory_order_acquire) != THREAD_NUM);
  uint64_t bgn, end;
  //-----
  // Bgn = rdtscp();
  bgn = rdtscp();
  for (;;) {
    // End = rdtscp();
    end = rdtscp();
    usleep(1000);
    if (chkClkSpan(bgn, end, EXTIME * 1000 * 1000 * CLOCK_PER_US)) break;
  }

  Bgn = bgn;
  End = end;
  Finish.store(true, std::memory_order_release);

  return nullptr;
}

static void *worker(void *arg) {
  int myid = *(int *) arg;
  uint64_t ctr(0);
  uint val_size = 64 * 1000;
  int *obVal;
  obVal = new int[val_size / 4];

  pid_t pid;
  cpu_set_t cpu_set;

  pid = syscall(SYS_gettid);
  CPU_ZERO(&cpu_set);
  // CPU_SET(*myid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);
  if (myid == 1)
    CPU_SET(1, &cpu_set);
  else if (myid == 2)
    CPU_SET(28, &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) ERR;

  unsigned int expected, desired;
  expected = Running.load(std::memory_order_acquire);
  for (;;) {
    desired = expected + 1;
    if (Running.compare_exchange_weak(expected, desired,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire))
      break;
  }

  // spin wait
  while (Running.load(std::memory_order_acquire) != THREAD_NUM) {
    _mm_pause();
  }

  for (;;) {
    // Counter.load(std::memory_order_acquire);
    // Counter.store(myid, std::memory_order_release);
    //++ctr;
    // for (uint i = 0; i < val_size/4; ++i)
    //  obVal[i] = myid;
    if (Finish.load(std::memory_order_relaxed)) break;
  }

  printf("Th #%d\t: %lu\n", myid, ctr);
  return nullptr;
}

static pthread_t threadCreate(int id) {
  pthread_t t;
  int *myid;

  try {
    myid = new int;
  } catch (bad_alloc) {
    ERR;
  }
  *myid = id;

  if (id == 0) {
    if (pthread_create(&t, nullptr, manager_worker, (void *) myid)) ERR;
  } else {
    if (pthread_create(&t, nullptr, worker, (void *) myid)) ERR;
  }

  return t;
}

int main(const int argc, const char *argv[]) {
  chkArg(argc, argv);

  pthread_t thread[THREAD_NUM];

  for (unsigned int i = 0; i < THREAD_NUM; ++i) thread[i] = threadCreate(i);

  for (unsigned int i = 0; i < THREAD_NUM; ++i)
    pthread_join(thread[i], nullptr);

  // prtRslt(Bgn, End);

  return 0;
}
