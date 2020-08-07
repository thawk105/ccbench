#include "include/result.hh"
#include "include/common.hh"

#include "../include/cache_line_size.hh"
#include "../include/result.hh"

using namespace std;

alignas(CACHE_LINE_SIZE) std::vector<Result> SiloResult;

void initResult() { SiloResult.resize(FLAGS_thread_num); }
