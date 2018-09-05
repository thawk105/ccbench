#include <iostream>
#include <random>
#include "include/procedure.hpp"
#include "include/common.hpp"
#include "include/debug.hpp"

using namespace std;

void
makeProcedure()
{
	try {
		Pro = new Procedure*[PRO_NUM];
		for (int i = 0; i < PRO_NUM; i++) {
			Pro[i] = new Procedure[MAX_OPE];
		}
	} catch (bad_alloc) {
		ERR;
	}

	random_device rnd;
	for (int i = 0; i < PRO_NUM; i++) {
		for (int j = 0; j < MAX_OPE; j++) {
			if ((rnd() % 100) < (READ_RATIO * 100))
				Pro[i][j].ope = Ope::READ;
			else
				Pro[i][j].ope = Ope::WRITE;
			Pro[i][j].key = rnd() % TUPLE_NUM;
			Pro[i][j].val = rnd() % (TUPLE_NUM * 10);
		}
	}
}
