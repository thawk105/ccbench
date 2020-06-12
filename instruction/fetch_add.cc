
#include <ctype.h>  //isdigit,
#include <pthread.h>
#include <string.h>       //strlen,
#include <sys/syscall.h>  //syscall(SYS_gettid),
#include <sys/types.h>    //syscall(SYS_gettid),
#include <time.h>
#include <unistd.h>  //syscall(SYS_gettid),
#include <xmmintrin.h>

#include <atomic>
#include <iostream>
#include <string>  //string
#include <thread>
#include <vector>

#include "../include/atomic_wrapper.hh"
#include "../include/cache_line_size.hh"
#include "../include/debug.hh"
#include "../include/int64byte.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"

using namespace std;

extern bool isReady(const std::vector<char> &readys);

extern void waitForReady(const std::vector<char> &readys);

extern void sleepMs(size_t ms);

alignas(CACHE_LINE_SIZE) size_t THREAD_NUM;
alignas(CACHE_LINE_SIZE) std::atomic <uint64_t> Cent_ctr(0);
#define EXTIME 3

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
  if (argc != 2) {
    cout << "usage: ./a.out THREAD_NUM" << endl;
    cout << "example: ./a.out 24" << endl;
    cout << "THREAD_NUM(int): total numbers of worker thread" << endl;
    cout << "argc == " << argc << endl;

    exit(0);
  }

  chkInt(argv[1]);

  THREAD_NUM = atoi(argv[1]);
  if (THREAD_NUM < 1) {
    cout << "THREAD_NUM must be larger than 0" << endl;
    ERR;
  }
}

void worker(size_t thid, char &ready, const bool &start, const bool &quit,
            uint64_t &count) {
  pid_t pid;
  cpu_set_t cpu_set;

  pid = syscall(SYS_gettid);
  CPU_ZERO(&cpu_set);
  CPU_SET(thid % sysconf(_SC_NPROCESSORS_CONF), &cpu_set);

  if (sched_setaffinity(pid, sizeof(cpu_set_t), &cpu_set) != 0) {
    ERR;
  }

  uint64_t lcount(0);
  storeRelease(ready, 1);
  while (!loadAcquire(start)) _mm_pause();
  while (!loadAcquire(quit)) {
    Cent_ctr.fetch_add(1);
    ++lcount;
  }

  storeRelease(count, lcount);
  return;
}

int main(const int argc, const char *argv[]) {
  chkArg(argc, argv);

  bool start(false), quit(false);
  std::vector <std::thread> ths;
  std::vector<char> readys(THREAD_NUM);
  std::vector <uint64_t> counts(THREAD_NUM);

  for (size_t i = 0; i < THREAD_NUM; ++i)
    ths.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit), std::ref(counts[i]));

  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < EXTIME; ++i) sleepMs(1000);
  storeRelease(quit, true);

  for (auto &t : ths) t.join();

  uint64_t sum(0);
  for (size_t i = 0; i < THREAD_NUM; ++i) {
    cout << "cpu#" << i << ":\t" << counts[i] << endl;
    sum += counts[i];
  }

  cout << sum / EXTIME << endl;

  return 0;
}
