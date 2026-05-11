#pragma once

#include "thread_management.hh"

#define OCC_MODE    1
#define OZE_MODE    2
#define TO_OCC_MODE 4
#define TO_OZE_MODE 8
#define DEFAULT_CC_MODE OZE_MODE
#define IS_OCC(mode) (mode & (OCC_MODE|TO_OCC_MODE))
#define IS_OZE(mode) (mode & (OZE_MODE|TO_OZE_MODE))

extern uint64_t TotalThreadNum;
extern ThreadManagementEntry **ThManagementTable;

inline bool all_threads_in(uint8_t mode, uint8_t *mode_array) {
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    if ((mode & mode_array[i]) == 0) {
      return false;
    }
  }
  return true;
}

inline bool all_other_threads_in(uint8_t mode, uint8_t *mode_array, uint8_t thread_id) {
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    if (thread_id == i) continue;
    if ((mode & mode_array[i]) == 0) {
      return false;
    }
  }
  return true;
}

inline bool some_threads_in(uint8_t mode, uint8_t *mode_array) {
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    if (mode & mode_array[i]) {
      return true;
    }
  }
  return false;
}

inline bool some_threads_in(uint8_t mode) {
  uint8_t mode_array[TotalThreadNum];
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    mode_array[i] = ThManagementTable[i]->atomicLoadMode();
  }
  return some_threads_in(mode, mode_array);
}

inline bool some_other_threads_in(uint8_t mode, uint8_t *mode_array, uint8_t thread_id) {
  for (unsigned int i = 0; i < TotalThreadNum; ++i) {
    if (thread_id == i) continue;
    if (mode & mode_array[i]) {
      return true;
    }
  }
  return false;
}
