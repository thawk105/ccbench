#include "include/debug.hpp"
#include "include/common.hpp"
#include "include/random.hpp"
#include "include/tuple.hpp"

using namespace std;

void makeDB() {
	Tuple *tmp;
	Xoroshiro128Plus rnd;
	rnd.init();

	try {
		if (posix_memalign((void**)&Table, 64, (TUPLE_NUM) * sizeof(Tuple)) != 0) ERR;
	} catch (bad_alloc) {
		ERR;
	}

	for (unsigned int i = 0; i < TUPLE_NUM; i++) {
		tmp = &Table[i];
		tmp->tidword = 1;
		tmp->tidword = tmp->tidword << 32;
		tmp->tidword = tmp->tidword | 0b010;
		tmp->key = i;
		tmp->val.store(rnd.next() % (TUPLE_NUM * 10), memory_order_release);
	}

}

