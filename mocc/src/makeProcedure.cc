#include <iostream>
#include <fstream>
#include <stdio.h>
#include <random>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"

using namespace std;

void makeProcedure() {
	try {
		Pro = new Procedure*[PRO_NUM];	
		for (unsigned int i = 0; i < PRO_NUM; ++i) {
			if (posix_memalign((void**)&Pro[i], 64, (MAX_OPE) * sizeof(Procedure)) != 0) ERR;
		}
	} 
	catch (bad_alloc) {
		cout << "memory exhausted" << endl;
		exit(1);
	}

	Xoroshiro128Plus rnd;
	rnd.init();

	for (unsigned int i = 0; i < PRO_NUM; ++i) {
		for (unsigned int j = 0; j < MAX_OPE; ++j) {
			if ((rnd.next() % 100) < (READ_RATIO * 100)) {
				Pro[i][j].ope = Ope::READ;
			}
			else {
				Pro[i][j].ope = Ope::WRITE;
			}
			Pro[i][j].key = rnd.next() % TUPLE_NUM;
			Pro[i][j].val = rnd.next() % (TUPLE_NUM*10);
		}

	}
}
