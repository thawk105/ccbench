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
			case Workload::R_ONLY:
				pro[i].ope = Ope::READ;
				break;
			case Workload::R_INTENS:
				if ((Rnd[thid].next() % 100) < 80) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
				break;
			case Workload::RW_EVEN:
				if ((Rnd[thid].next() % 100) < 50) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
			case Workload::W_INTENS:
				break;
				if ((Rnd[thid].next() % 100) < 20) {
					pro[i].ope = Ope::READ;
				} else {
					pro[i].ope = Ope::WRITE;
				} 
				break;
			case Workload::W_ONLY:
				pro[i].ope = Ope::WRITE;
				break;
		}
		pro[i].key = Rnd[thid].next() % TUPLE_NUM;
		pro[i].val = Rnd[thid].next() % TUPLE_NUM;
	}
}
