#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/transaction.hpp"
#include <atomic>

using namespace std;

int
Transaction::tread(int key)
{
	return Table[key].val.load(std::memory_order_acquire);
}

void
Transaction::twrite(int key, int val)
{
	Table[key].val.store(NULL, std::memory_order_release);
}
