#include <execinfo.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "include/debug.hh"

std::vector<std::string> my_backtrace() {
    auto trace_size = 10;
    void* trace[trace_size];
    auto size = backtrace(trace, trace_size);
    auto symbols = backtrace_symbols(trace, size);
    std::vector<std::string> result(symbols, symbols + size);
    free(symbols);
    return result;
}

void my_assert(bool expr) {
    if (unlikely(!expr)) {
        auto vec = my_backtrace();
        int count = vec.size();
        for (auto v : vec) {
            count--;
            std::cout << "backtrace [" << count << "] " << v << std::endl;
        }
        abort();
    }
}
