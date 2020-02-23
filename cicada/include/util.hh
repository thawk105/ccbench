#pragma once

#include <vector>

#include "../../include/backoff.hh"

extern void chkArg(const int argc, char *argv[]);

extern void deleteDB();

extern void displayDB();

extern void displayMinRts();

extern void displayMinWts();

extern void displayThreadWtsArray();

extern void displayThreadRtsArray();

extern void displaySLogSet();

extern void leaderWork([[maybe_unused]] Backoff &backoff, [[maybe_unused]] std::vector<Result> &res);

extern void makeDB(uint64_t *initial_wts);

extern void partTableDelete([[maybe_unused]] size_t thid, uint64_t start, uint64_t end);

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t initts, uint64_t start, uint64_t end);

extern void ShowOptParameters();
