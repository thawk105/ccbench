#pragma once

#include <iostream>
#include <mutex>

#ifdef GLOBAL_VALUE_DEFINE
std::mutex cout_mutex;
void dump(int thid, std::string s) {
  std::lock_guard<std::mutex> lock(cout_mutex);
  std::cout << "[th#" << thid << "]: " << s << std::endl;
}
#else
extern std::mutex cout_mutex;
extern void dump(int thid, std::string s);
#endif

#define CCC(val)                                                              \
  do {                                                                        \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %c\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                         \
    fflush(stderr);                                                           \
  } while (0)
#define DDD(val)                                                              \
  do {                                                                        \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %d\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                         \
    fflush(stderr);                                                           \
  } while (0)
#define PPP(val)                                                              \
  do {                                                                        \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %p\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                         \
    fflush(stderr);                                                           \
  } while (0)
#define LLL(val)                                                               \
  do {                                                                         \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %ld\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                          \
    fflush(stderr);                                                            \
  } while (0)
#define SSS(val)                                                              \
  do {                                                                        \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %s\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                         \
    fflush(stderr);                                                           \
  } while (0)
#define FFF(val)                                                              \
  do {                                                                        \
    fprintf(stderr, "%ld %16s %4d %16s %16s: %f\n", (long int)pthread_self(), \
            __FILE__, __LINE__, __func__, #val, val);                         \
    fflush(stderr);                                                           \
  } while (0)
#define NNN                                                                    \
  do {                                                                         \
    fprintf(stderr, "%ld %16s %4d %16s\n", (long int)pthread_self(), __FILE__, \
            __LINE__, __func__);                                               \
    fflush(stderr);                                                            \
  } while (0)
#define ERR          \
  do {               \
    perror("ERROR"); \
    NNN;             \
    exit(1);         \
  } while (0)
#define RERR(fd)          \
  do {                    \
    perror("Recv Error"); \
    NNN;                  \
    close(fd);            \
    pthread_exit(NULL);   \
  } while (0)
#define ERR2            \
  do {                  \
    perror("ERROR");    \
    NNN;                \
    pthread_exit(NULL); \
  } while (0)

#define FCN cout << "file can't open.\n"
