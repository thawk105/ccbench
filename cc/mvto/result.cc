#include "include/result.hh"
#include "include/common.hh"

using namespace std;

alignas(CACHE_LINE_SIZE) std::vector<Result> MvtoResult;

void initResult() { MvtoResult.resize(TotalThreadNum); }
