
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

#define GB std::pow(2,30)
#define MB std::pow(2,20)
#define K 1000

#define CLOCKS_PER_US 2100
// マイクロ秒あたりのクロック数. CPU MHz.
#define EX_TIME 3
// 実行時間. 3 seconds.

#define LOOP 1000

using std::cout;
using std::endl;

class Result {
public:
  uint64_t readSeqMemband = 0;
  uint64_t writeSeqMemband = 0;
  uint64_t readRanMemband = 0;
  uint64_t writeRanMemband = 0;
  int thid = 0;
};

unsigned int TH_NUM;

std::atomic<unsigned int> Running(0);

extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);
extern bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold);

uint64_t
sequentialReadBench(unsigned int thid)
{
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }

  uint64_t start, stop;
  start = rdtscp();
  for (int i = 0; i < GB; i+=8) {
    memcpy(&Rec[0], &Array[i], 64);
  }
  stop = rdtscp();

  double readBytes = GB * 8;
  double readMbytes = readBytes / K / K;
  double exsec = (double)(stop - start)/(double)CLOCKS_PER_US / K / K;
  double mbytespersec = readMbytes / exsec;
  printf("sequential read BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  free(Array);
  free(Rec);
  return (uint64_t)mbytespersec;
}

uint64_t
sequentialWriteBench(unsigned int thid)
{
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }
  
  uint64_t start, stop;
  start = rdtscp();
  for (int i = 0; i < GB; i+=8) {
    memcpy(&Array[i], Rec, 64);
  }
  stop = rdtscp();

  double writeBytes = GB * 8;
  double writeMbytes = writeBytes / K / K;
  double exsec = (stop - start)/CLOCKS_PER_US / K / K;
  double mbytespersec = writeMbytes / exsec;
  printf("sequential write BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  free(Array);
  free(Rec);
  return (uint64_t)mbytespersec;
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
randomReadBench(unsigned int thid)
{
  Xoroshiro128Plus rnd;
  rnd.init();
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }

  uint64_t start, stop;
  start = rdtscp();
  for (int i = 0; i < GB; i+=8) {
    memcpy(&Rec[0], &Array[rnd.next() >> 34], 64);
  }
  stop = rdtscp();

  double readBytes = GB * 8;
  double readMbytes = readBytes / K / K;
  double exsec = (double)(stop - start)/(double)CLOCKS_PER_US / K / K;
  double mbytespersec = readMbytes / exsec;
  printf("random read BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  free(Array);
  free(Rec);
  return (uint64_t)mbytespersec;
}

uint64_t
randomWriteBench(unsigned int thid)
{
  Xoroshiro128Plus rnd;
  rnd.init();
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, GB * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }
  uint64_t start, stop;
  start = rdtscp();
  for (int i = 0; i < GB; i+=8) {
    memcpy(&Array[rnd.next() >> 34], Rec, 64);
  }
  stop = rdtscp();

  double writeBytes = GB * 8;
  double writeMbytes = writeBytes / K / K;
  double exsec = (stop - start)/CLOCKS_PER_US / K / K;
  double mbytespersec = writeMbytes / exsec;
  printf("random write BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  free(Array);
  free(Rec);
  return (uint64_t)mbytespersec;
}
void
rdtscBench()
{
  uint64_t start, stop;
  uint64_t max = MB;
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
  uint64_t max = MB;
  start = rdtscp();
  for (unsigned int i = 0; i < max; ++i)
    rdtscp();
  stop = rdtscp();

  cout << "rdtscpBench[clocks] :\t" << (stop - start)/max << endl;
}

void *
worker(void *args)
{
  Result &res = *(Result *)(args);
  setThreadAffinity(res.thid);
  waitForReadyOfAllThread(Running, TH_NUM);
  
  res.readSeqMemband = sequentialReadBench(res.thid);
  res.writeSeqMemband = sequentialWriteBench(res.thid);
  res.readRanMemband = randomReadBench(res.thid);
  res.writeRanMemband = randomWriteBench(res.thid);

  return nullptr;
}

int
main(int argc, char *argv[])
{
  TH_NUM = atoi(argv[1]);
  Result res[TH_NUM];
  xoroshiro128PlusBench();
  rdtscBench();
  rdtscpBench();
  //
  pthread_t thread[TH_NUM];
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    int ret;
    res[i].thid = i;
    ret = pthread_create(&thread[i], NULL, worker, (void *)(&res[i]));
    if (ret) ERR;
  }

  for (unsigned int i = 0; i < TH_NUM; ++i) {
    pthread_join(thread[i], NULL);
  }

  uint64_t membandSum[4] = {};
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    membandSum[0] += res[i].readSeqMemband;
    membandSum[1] += res[i].writeSeqMemband;
    membandSum[2] += res[i].readRanMemband;
    membandSum[3] += res[i].writeRanMemband;
  }

  cout << "readSeqMemband[Mbytes/sec]:\t" << membandSum[0] << endl;
  cout << "writeSeqMemband[Mbytes/sec]:\t" << membandSum[1] << endl;
  cout << "readRanMemband[Mbytes/sec]:\t" << membandSum[2] << endl;
  cout << "writeRanMemband[Mbytes/sec]:\t" << membandSum[3] << endl;
  return 0;
}
