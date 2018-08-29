#ifndef TRANSACTION_HPP
#define	TRANSACTION_HPP

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include "tuple.hpp"
#include "procedure.hpp"
#include "common.hpp"
#include <iostream>
#include <set>
#include <map>

class Transaction {
public:
	std::map<int, uint64_t, std::less<int>, tbb::scalable_allocator<uint64_t>> readSet;
	std::map<int, unsigned int, std::less<int>, tbb::scalable_allocator<unsigned int>> writeSet;
	int thid;

	int tread(int key);
	void twrite(int key, int val);
	bool validationPhase();
	void abort();
	void writePhase();
	void lockWriteSet();
	void unlockWriteSet();
};

#endif	//	TRANSACTION_HPP
