
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

#define LEN 64*4*1000*1000/4
// キャッシュサイズの4倍程度をアレイサイズとして確保することを想定する．
// ?*?*? がキャッシュサイズ．uint64_t (8byte object) なので /4.

#define CLOCKS_PER_US 2100
// マイクロ秒あたりのクロック数. CPU MHz.

#define LOOP 100 
// 統計的試行回数．この回数だけ連続実行し，実行時間をこの数で割る．

using std::cout;
using std::endl;

class Result {
public:
  uint64_t readMemband = 0;
  uint64_t writeMemband = 0;
  int thid = 0;
};

unsigned int TH_NUM;
std::atomic<unsigned int> Running(0);

extern void waitForReadyOfAllThread(std::atomic<unsigned int> &running, const unsigned int thnum);

uint64_t
sequentialReadBench(unsigned int thid)
{
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, LEN * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, LEN * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }

  for (int i = 0; i < LEN; ++i) {
    Array[i] = 1;
    Rec[i] = 2;
  }

  // キャッシュサイズの二倍程度でアレイサイズを確保する．
  uint64_t start, stop;

  start = rdtsc();
  for (int h = 0; h < LOOP; ++h) {
    for (int i = 0; i < LEN; i+=8) {
      memcpy(&Rec[0], &Array[i], 64);
    }
  }
  stop = rdtsc();

  double readbytesPerLoop = LEN * 8;
  double readMbytesPerLoop = readbytesPerLoop / 1000.0 / 1000.0;
  double exsec = (double)(stop - start)/(double)CLOCKS_PER_US/1000.0/1000.0;
  double mbytespersec = readMbytesPerLoop * LOOP / exsec;
  printf("sequential read BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  return (uint64_t)mbytespersec;
}

uint64_t
sequentialWriteBench(unsigned int thid)
{
  uint64_t *Array;
  uint64_t *Rec; // read を受け取るためのアレイ．

  try {
    if (posix_memalign((void**)&Array, CACHE_LINE_SIZE, LEN * sizeof(uint64_t)) != 0) ERR;
    if (posix_memalign((void**)&Rec, CACHE_LINE_SIZE, LEN * sizeof(uint64_t)) != 0) ERR;
  } catch (std::bad_alloc) {
    ERR;
  }

  for (int i = 0; i < LEN; ++i) {
    Array[i] = 1;
    Rec[i] = 2;
  }

  uint64_t start, stop;

  start = rdtsc();
  for (int h = 0; h < LOOP; ++h) {
    for (int i = 0; i < LEN; i+=8) {
      memcpy(&Array[i], Rec, 64);
    }
  }
  stop = rdtsc();

  double writebytesPerLoop = LEN * 8;
  double writeMbytesPerLoop = writebytesPerLoop / 1000.0 / 1000.0;
  double exsec = (stop - start)/CLOCKS_PER_US / 1000.0 / 1000.0;
  double mbytespersec = writeMbytesPerLoop * LOOP / exsec;
  printf("sequential write BW, Th#%d:\t%lu\n", thid, (uint64_t)mbytespersec);
  return (uint64_t)mbytespersec;
}

void
xoroshiro128PlusBench()
{
  Xoroshiro128Plus rnd;
  rnd.init();
  
  uint64_t start, stop;
  start = rdtsc();
  for (int i = 0; i < LOOP*LOOP*LOOP; ++i)
    rnd.next();
  stop = rdtsc();

  cout << "xoroshiro128PlusBench[ns] :\t" << (double)(stop - start)/(double)(LOOP*LOOP*LOOP)/(double)(CLOCKS_PER_US / 1000) << endl;
}

void
rdtscBench()
{
  uint64_t start, stop;
  start = rdtsc();
  for (int i = 0; i < LOOP*LOOP; ++i)
    rdtsc();
  stop = rdtsc();

  cout << "rdtscBench[clocks] :\t" << (stop - start)/LOOP/LOOP << endl;
}

void *
worker(void *args)
{
  Result &res = *(Result *)(args);
  setThreadAffinity(res.thid);
  waitForReadyOfAllThread(Running, TH_NUM);
  
  res.readMemband = sequentialReadBench(res.thid);
  res.writeMemband = sequentialWriteBench(res.thid);

  return nullptr;
}

int
main(int argc, char *argv[])
{
  TH_NUM = atoi(argv[1]);
  Result res[TH_NUM];
  //xoroshiro128PlusBench();
  //rdtscBench();
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

  uint64_t membandSum[2] = {};
  for (unsigned int i = 0; i < TH_NUM; ++i) {
    membandSum[0] += res[i].readMemband;
    membandSum[1] += res[i].writeMemband;
  }

  cout << "readMemband[Mbytes/sec]:\t" << membandSum[0] << endl;
  cout << "writeMemband[Mbytes/sec]:\t" << membandSum[1] << endl;
  return 0;
}
