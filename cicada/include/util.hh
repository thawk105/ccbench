#pragma once

#include <vector>

#include "../../include/backoff.hh"

extern void chkArg();

extern void deleteDB();

[[maybe_unused]] extern void displayDB();

[[maybe_unused]] extern void displayMinRts();

[[maybe_unused]] extern void displayMinWts();

extern void displayParameter();

[[maybe_unused]] extern void displaySLogSet();

[[maybe_unused]] extern void displayThreadWtsArray();

[[maybe_unused]] extern void displayThreadRtsArray();

extern void leaderWork([[maybe_unused]] Backoff &backoff);

extern void makeDB(uint64_t *initial_wts);

extern void partTableDelete([[maybe_unused]] size_t thid, uint64_t start,
                            uint64_t end);

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t initts,
                          uint64_t start, uint64_t end);

extern void ShowOptParameters();
