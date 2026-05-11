#pragma once

#include <vector>

#include "../../include/backoff.hh"

extern void init(int32_t table_num);

extern void deleteDB();

extern void displayDB();

extern void displayParameter();

extern void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);

extern void makeDB(uint64_t *initial_wts);

extern void partTableDelete([[maybe_unused]] size_t thid, uint64_t start,
                            uint64_t end);

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t initts,
                          uint64_t start, uint64_t end);

extern void ShowOptParameters();
