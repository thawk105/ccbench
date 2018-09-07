#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <vector>

#include "lock.hpp"
#include "tuple.hpp"

using namespace std;

enum class TransactionStatus : uint8_t {
	inFlight,
	committed,
	aborted,
};

class Transaction {
public:
	int thid;
	std::vector<RWLock*> r_lockList;
	std::vector<RWLock*> w_lockList;
	TransactionStatus status = TransactionStatus::inFlight;

	vector<SetElement> readSet;
	vector<SetElement> writeSet;

	Transaction(int myid) {
		this->thid = myid;
		readSet.reserve(MAX_OPE);
		writeSet.reserve(MAX_OPE);
		r_lockList.reserve(MAX_OPE);
		w_lockList.reserve(MAX_OPE);
	}

	void abort();
	void commit();
	void tbegin();
	int tread(unsigned int key);
	void twrite(unsigned int key, unsigned int val);
	void unlock_list();
};

#endif	//	TRANSACTION_HPP
