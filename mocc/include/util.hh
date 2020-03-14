#pragma once

extern void chkArg();

extern bool chkEpochLoaded();

extern void displayDB();

extern void displayParameter();

extern void displayLockedTuple();

extern void leaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop,
                       [[maybe_unused]] Result &res);

extern void makeDB();

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t start,
                          uint64_t end);

extern void ShowOptParameters();
