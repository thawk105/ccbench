#pragma once

#include <vector>

#include "../../include/backoff.hh"

extern void chkArg();

extern void deleteDB();

extern void displayDB();

extern void displayMinRts();

extern void displayMinWts();

extern void displayParameter();

extern void displaySLogSet();

extern void displayThreadWtsArray();

extern void displayThreadRtsArray();

extern void leaderWork([[maybe_unused]] Backoff &backoff);

extern void makeDB(uint64_t *initial_wts);

extern void partTableDelete([[maybe_unused]] size_t thid, uint64_t start,
                            uint64_t end);

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t initts,
                          uint64_t start, uint64_t end);

extern void ShowOptParameters();
