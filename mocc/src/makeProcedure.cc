#include <iostream>
#include <fstream>
#include <stdio.h>
#include <random>
#include "include/common.hpp"
#include "include/debug.hpp"
#include "include/procedure.hpp"
#include "include/random.hpp"

using namespace std;

void makeProcedure(Procedure *pro, unsigned int thid) {
	for (unsigned int i = 0; i < MAX_OPE; ++i) {
		switch (WORKLOAD) {
			case 1:
				pro[i].ope = Ope::READ;
				break;
			case 2:
				if ((Rnd[thid].next() % 100) < R_INTENS) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
				break;
			case 3:
				if ((Rnd[thid].next() % 100) < RW_EVEN) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
			case 4:
				break;
				if ((Rnd[thid].next() % 100) < W_INTENS) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
				break;
			case 5:
				pro[i].ope = Ope::WRITE;
				break;
		}
		pro[i].key = Rnd[thid].next() % TUPLE_NUM;
		pro[i].val = Rnd[thid].next() % TUPLE_NUM;
	}
}
