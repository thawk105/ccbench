#pragma once

extern void chkArg();

extern void displayDB();

extern void displayParameter();

extern void leaderWork(GarbageCollection &gcob);

extern void makeDB();

extern void naiveGarbageCollection(const bool &quit);

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t start,
                          uint64_t end);

extern void ShowOptParameters();
