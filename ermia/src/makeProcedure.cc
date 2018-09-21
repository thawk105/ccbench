#include <iostream>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"

using namespace std;

void
makeProcedure(Procedure *pro, Xoroshiro128Plus &rnd)
{
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		switch (WORKLOAD) {
			case 1:
				pro[i].ope = Ope::READ;
				break;
			case 2:
				if ((rnd.next() % 100) < R_INTENS) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 3:
				if ((rnd.next() % 100) < RW_EVEN) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 4:
				if ((rnd.next() % 100) < W_INTENS) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				}
				break;
			case 5:
				pro[i].ope = Ope::WRITE;
				break;
		}
		pro[i].key = rnd.next() % TUPLE_NUM;	// range of key 1 ~ TUPLE_NUM
		pro[i].val = rnd.next() % TUPLE_NUM;
	}
}
