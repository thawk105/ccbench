#include <random>
#include "include/tuple.hpp"
#include "include/debug.hpp"
#include "include/common.hpp"

using namespace std;

void makeDB() {
	Tuple *tmp;
	random_device rnd;

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->tsword = 0;
		tmp->key = i;
		tmp->val.store(rnd() % (TUPLE_NUM * 10), memory_order_release);
	}

}

