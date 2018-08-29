#ifndef TRANSACTION_HPP
#define	TRANSACTION_HPP

#include "tuple.hpp"
#include "procedure.hpp"
#include "common.hpp"
#include "timeStamp.hpp"
#include "version.hpp"

#include "/home/tanabe/package/tbb/include/tbb/scalable_allocator.h"

#include <iostream>
#include <map>

using namespace std;

enum class TransactionStatus : uint8_t {
	invalid,
	commit,
	abort,
};

class Transaction {
public:
	TransactionStatus status = TransactionStatus::invalid;
	TimeStamp *rts;
	TimeStamp *wts;
	bool ronly = false;
	unsigned int transactionNum;
	std::map<int, Version *, less<int>, tbb::scalable_allocator<Version *>> readSet;
	std::map<int, ElementSet *, less<int>, tbb::scalable_allocator<ElementSet *>> writeSet;
	//ElementSet class include Version *sourceObject, Version *newObject.
	int eleNum_wset = 0;	//element number of wset, used by pwal

	uint64_t start, stop;
	unsigned int thid;

	void tbegin(TimeStamp &thrts, TimeStamp &thwts, bool &firstFlag, const int &thid, const unsigned int transactionNum);
	int tread(int key);
	void twrite(int key, int val);
	bool validation();
	void writePhase();
	void noticeToClients();
	void swal();
	void pwal();	//parallel write ahead log.
	void precpv();	//pre-commit pending versions
	void cpv();	//commit pending versions
	void gcpv();	//group commit pending versions
	bool chkGcpvTimeout();
	void earlyAbort();
	void abort();
	void wSetClean();
	void displayWset();
	void mainte(int proIndex);	//maintenance
};

#endif	//	TRANSACTION_HPP
