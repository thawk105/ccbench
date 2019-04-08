
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <atomic>
#include <iostream>

#include "../include/cache_line_size.hpp"
#include "../include/cpu.hpp"
#include "../include/debug.hpp"
#include "../include/random.hpp"
#include "../include/tsc.hpp"

#define GiB 1073741824
#define MiB 1048576
#define KiB  1024

#define CLOCKS_PER_US 2100
// マイクロ秒あたりのクロック数. CPU MHz.
#define EX_TIME 3
// 実行時間. 3 seconds.

#define LOOP 1000

using std::cout;
using std::endl;

class Result {
public:
  uint64_t seqReadMemband = 0;
  uint64_t seqWriteMemband = 0;
  uint64_t ranReadMemband = 0;
  uint64_t ranWriteMemband = 0;
  int thid = 0;
};

unsigned int TH_NUM;
unsigned int MALLOC_SIZE; // GiB

std::atomic<unsigned int> RunRanReadBench(0);
std::atomic<unsigned int> RunRanWriteBench(0);
std::atomic<unsigned int> RunSeqReadBench(0);
std::atomic<unsigned int> RunSeqWriteBench(0);

std::atomic<bool> FinRanReadBench(false);
std::atomic<bool> FinRanWriteBench(false);
std::atomic<bool> FinSeqReadBench(false);
std::atomic<bool> FinSeqWriteBench(false);

extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);

uint64_t
seqReadBench(unsigned int thid, char *Array, char *Rec)
{
  const uint64_t max = MALLOC_SIZE * GiB;
  uint64_t i = 0;
  for (uint64_t i = 0; i < max; i+=CACHE_LINE_SIZE)
    memcpy(&Rec[0], &Array[i], CACHE_LINE_SIZE);

  uint64_t nr_cl = 0;
  i = 0;
  waitForReadyOfAllThread(RunSeqReadBench, TH_NUM+1);
  for (;;) {
    memcpy(&Rec[0], &Array[i], CACHE_LINE_SIZE);
    ++nr_cl;
    if (FinSeqReadBench.load()) break;
    i += 64;
    if (i == max) {
      i = 0;
    }
  }

  double readMebiBytes = (nr_cl * CACHE_LINE_SIZE) / (double)MiB;
  double exsec = EX_TIME;
  uint64_t mibps = readMebiBytes / exsec;
  printf("sequentialReadBW[MiB/s],Th#%d:\t%lu\n", thid, mibps);
  return mibps;
}

uint64_t
seqWriteBench(unsigned int thid, char *Array, char *Rec)
{
  const uint64_t max = MALLOC_SIZE * GiB;
  uint64_t i = 0;
  for (uint64_t i = 0; i < max; i+=CACHE_LINE_SIZE)
    memcpy(&Rec[0], &Array[i], CACHE_LINE_SIZE);

  uint64_t nr_cl = 0;
  i = 0;
  waitForReadyOfAllThread(RunSeqWriteBench, TH_NUM+1);
  for (;;) {
    memcpy(&Array[i], Rec, CACHE_LINE_SIZE);
    ++nr_cl;
    if (FinSeqWriteBench.load()) break;
    i += 64;
    if (i == max) {
      i = 0;
    }
  }

  double writeMebiBytes = (nr_cl * CACHE_LINE_SIZE) / (double)MiB;
  double exsec = EX_TIME;
  uint64_t mibps = writeMebiBytes / exsec;
  printf("sequentialWriteBW[MiB],Th#%d:\t%lu\n", thid, mibps);
  return mibps;
}

void
xoroshiro128PlusBench()
{
  Xoroshiro128Plus rnd;
  rnd.init();
  
  uint64_t start, stop;
  start = rdtscp();
  for (;;) {
    for (int i = 0; i < LOOP; ++i)
      rnd.next();
    stop = rdtscp();
    if (chkClkSpan(start, stop, CLOCKS_PER_US * 1000 * 1000 * EX_TIME)) goto STOP;
  }
STOP:

  cout << "xoroshiro128PlusBench[ns] :\t" << (double)(stop - start)/(double)(LOOP*LOOP*LOOP)/(double)(CLOCKS_PER_US / 1000) << endl;
}

uint64_t
ranReadBench(unsigned int thid, char *Array, char *Rec)
{
  Xoroshiro128Plus rnd;
  rnd.init();
  memcpy(&Rec[0], &Array[rnd.next() >> 34], 64);

  const uint64_t max = MALLOC_SIZE * GiB;
  for (uint64_t i = 0; i < max; i+=CACHE_LINE_SIZE)
    memcpy(&Rec[0], &Array[i], CACHE_LINE_SIZE);

  uint64_t nr_cl = 0;
  uint8_t sw = 34 + (MALLOC_SIZE / 2);
  waitForReadyOfAllThread(RunRanReadBench, TH_NUM+1);
  for (;;) {
    memcpy(&Rec[0], &Array[rnd.next() >> sw], CACHE_LINE_SIZE);
    ++nr_cl;
    if (FinRanReadBench.load()) break;
  }

  double readMebiBytes = (nr_cl * CACHE_LINE_SIZE) / (double)MiB;
  double exsec = EX_TIME;
  uint64_t mibps = readMebiBytes / exsec;
  printf("randomeReadBW[MiB/s],Th#%d:\t%lu\n", thid, mibps);
  return mibps;
}

uint64_t
ranWriteBench(unsigned int thid, char *Array, char *Rec)
{
  Xoroshiro128Plus rnd;
  rnd.init();

  const uint64_t max = MALLOC_SIZE * GiB;
  for (uint64_t i = 0; i < max; i+=CACHE_LINE_SIZE)
    memcpy(&Rec[0], &Array[i], CACHE_LINE_SIZE);

  uint64_t nr_cl = 0;
  uint8_t sw = 34 + (MALLOC_SIZE / 2);
  waitForReadyOfAllThread(RunRanWriteBench, TH_NUM+1);
  for (;;) {
    memcpy(&Array[rnd.next() >> sw], Rec, CACHE_LINE_SIZE);
    ++nr_cl;
    if (FinRanWriteBench.load()) break;
  }

  double writeMebiBytes = (nr_cl * CACHE_LINE_SIZE) / (double)MiB;
  double exsec = EX_TIME;
  uint64_t mibps = writeMebiBytes / exsec;
  printf("randomWriteBW[MiB],Th#%d:\t%lu\n", thid, mibps);
  return mibps;
}

void
rdtscBench()
{
  uint64_t start, stop;
  uint64_t max = MiB;
  start = rdtscp();
  for (uint64_t i = 0; i < max; ++i)
    rdtsc();
  stop = rdtscp();

  cout << "rdtscBench[clocks] :\t" << (stop - start)/max << endl;
}

void
rdtscpBench()
{
  uint64_t start, stop;
  uint64_t max = MiB;
  start = rdtscp();
  for (unsigned int i = 0; i < max; ++i)
    rdtscp();
  stop = rdtscp();

  cout << "rdtscpBench[clocks] :\t" << (stop - start)/max << endl;
}

void *
leader(void *args)
{
  waitForReadyOfAllThread(RunSeqReadBench, TH_NUM+1);
  sleep(EX_TIME);
  FinSeqReadBench.store(true, std::memory_order_release);

  waitForReadyOfAllThread(RunSeqWriteBench, TH_NUM+1);
  sleep(EX_TIME);
  FinSeqWriteBench.store(true, std::memory_order_release);

  waitForReadyOfAllThread(RunRanReadBench, TH_NUM+1);
  sleep(EX_TIME);
  FinRanReadBench.store(true, std::memory_order_release);

  waitForReadyOfAllThread(RunRanWriteBench, TH_NUM+1);
  sleep(EX_TIME);
  FinRanWriteBench.store(true, std::memory_order_release);

  return args;
  // 何を返しても良い
  // -Wunused-parameter による
  // warning を抑えるため．
}

void *
worker(void *args)
{
  Result &res = *(Result *)(args);
  setThreadAffinity(res.thid);
  
  char *Array;
  char *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, GiB * MALLOC_SIZE * sizeof(char)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, CACHE_LINE_SIZE) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }

  res.seqReadMemband = seqReadBench(res.thid, Array, Rec);
  res.seqWriteMemband = seqWriteBench(res.thid, Array, Rec);
  res.ranReadMemband = ranReadBench(res.thid, Array, Rec);
  res.ranWriteMemband = ranWriteBench(res.thid, Array, Rec);

  free(Array);
  free(Rec);
  return nullptr;
}

int
main(int argc, char *argv[])
{
  if (argc != 3) ERR;
  TH_NUM = atoi(argv[1]);
  MALLOC_SIZE = atoi(argv[2]);

  Result res[TH_NUM];
  xoroshiro128PlusBench();
  rdtscBench();
  rdtscpBench();
  
  pthread_t lth;
  if (pthread_create(&lth, NULL, leader, NULL))
    ERR;

  pthread_t thread[TH_NUM];
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    int ret;
    res[i].thid = i;
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&res[i]));
    if (ret) ERR;
  }

  pthread_join(lth, NULL);
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    pthread_join(thread[i], NULL);
  }

  uint64_t membandSum[4] = {};
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    membandSum[0] += res[i].seqReadMemband;
    membandSum[1] += res[i].seqWriteMemband;
    membandSum[2] += res[i].ranReadMemband;
    membandSum[3] += res[i].ranWriteMemband;
  }

  cout << "sequentialReadMemband[MiB/s]:\t" << membandSum[0] << endl;
  cout << "sequentialWriteMemband[MiB/s]:\t" << membandSum[1] << endl;
  cout << "randomReadMemband[MiB/s]:\t" << membandSum[2] << endl;
  cout << "randomWriteMemband[MiB/s]:\t" << membandSum[3] << endl;

  return 0;
}
