#ifndef TRANSACTION_HPP
#define	TRANSACTION_HPP

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include "tuple.hpp"
#include "procedure.hpp"
#include "common.hpp"
#include <iostream>
#include <set>
#include <vector>

using namespace std;

class Transaction {
public:
	vector<ReadElement> readSet;
	vector<WriteElement> writeSet;

	Transaction(int thid) {
		readSet.reserve(MAX_OPE);
		writeSet.reserve(MAX_OPE);

		this->thid = thid;
	}

	int thid;

	int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
	bool validationPhase();
	void abort();
	void writePhase();
	void lockWriteSet();
	void unlockWriteSet();
};

#endif	//	TRANSACTION_HPP
