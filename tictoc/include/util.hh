#pragma once

#include <cstdint>

extern void chkArg(const int argc, char* argv[]);

extern void displayDB();

extern void partTableInit([[maybe_unused]] std::size_t thid, uint64_t start, uint64_t end);

extern void makeDB();

