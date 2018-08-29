#include <random>
#include "include/tuple.hpp"
#include "include/debug.hpp"
#include "include/common.hpp"

using namespace std;

void makeDB() {
	Tuple *tmp;
	random_device rnd;

	try {
		if (posix_memalign((void**)&Table, 2*8*64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (int i = 1; i <= TUPLE_NUM; i++) {
		tmp = &Table[i % TUPLE_NUM];
		tmp->tidword = 1;
		tmp->tidword = tmp->tidword << 32;
		tmp->tidword = tmp->tidword | 0b010;
		tmp->key = i;
		tmp->val.store(rnd() % (TUPLE_NUM * 10), memory_order_release);
	}

}

