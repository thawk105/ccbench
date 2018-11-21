#include <fstream>
#include <iostream>
#include <random>
#include <stdio.h>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"

using namespace std;

void 
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
	bool ronly = true;

	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		if ((rnd.next() % 10) < RRATIO) {
			pro[i].ope = Ope::READ;
		} else {
			ronly = false;
			pro[i].ope = Ope::WRITE;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
	
	if (ronly) pro[0].ronly = true;
	else pro[0].ronly = false;
}
