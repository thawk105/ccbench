#include <random>
#include "include/common.hpp"
#include "include/debug.hpp"
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

	for (unsigned int i = 0; i < TUPLE_NUM; ++i) {
		tmp = &Table[i];

		tmp->tidword.epoch = 1;
		tmp->tidword.lock = 0;
		tmp->tidword.tid = 0;

		tmp->epotemp.epoch = 1;
		tmp->epotemp.temp = 0;

		tmp->key = i;
		tmp->val = rnd.next() % TUPLE_NUM;
	}

}

