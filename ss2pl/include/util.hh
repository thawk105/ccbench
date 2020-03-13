#pragma once

extern void chkArg(const int argc, const char *argv[]);

extern void displayDB();

extern void makeDB();

extern void partTableInit([[maybe_unused]] size_t thid, uint64_t start, uint64_t end);

extern void ShowOptParameters();
