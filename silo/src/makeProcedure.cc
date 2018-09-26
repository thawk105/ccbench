#include <iostream>
#include <fstream>
#include <stdio.h>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"

using namespace std;

void makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd) {
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		switch (WORKLOAD) {
			case 0:
				pro[i].ope = Ope::READ;
				break;
			case 1:
				if ((rnd.next() % 100) < 80) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 2:
				if ((rnd.next() % 100) < 50) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 3:
				if ((rnd.next() % 100) < 20) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 4:
				pro[i].ope = Ope::WRITE;
				break;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
}
