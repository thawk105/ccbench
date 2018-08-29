#include <iostream>
#include <fstream>
#include <stdio.h>
#include <random>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"

using namespace std;

void makeProcedure() {
	try {
		Pro = new Procedure*[PRO_NUM];	
		for (unsigned int i = 0; i < PRO_NUM; i++) {
			//Pro[i] = new Procedure[MAX_OPE];
			if (posix_memalign((void**)&Pro[i], 2*8*64, (MAX_OPE) * sizeof(Procedure)) != 0) ERR;
		}
	} 
	catch (bad_alloc) {
		cout << "memory exhausted" << endl;
		exit(1);
	}

	random_device rnd;
	for (unsigned int i = 0; i < PRO_NUM; i++) {
		for (unsigned int j = 0; j < MAX_OPE; j++) {
			if ((rnd() % 100) < (READ_RATIO * 100))
				Pro[i][j].ope = Ope::READ;
			else
				Pro[i][j].ope = Ope::WRITE;
			Pro[i][j].key = rnd() % TUPLE_NUM + 1;
			Pro[i][j].val = rnd() % (TUPLE_NUM*10);
		}

	}
}
