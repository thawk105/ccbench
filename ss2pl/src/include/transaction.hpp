#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include <map>
#include <vector>

#include "lock.hpp"
#include "tuple.hpp"

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

	std::map<int, int> readSet;
	std::map<int, int> writeSet;

	void abort();
	void commit();
	void tbegin(int myid);
	int tread(int key);
	void twrite(int key, int val);
	void unlock_list();
};

#endif	//	TRANSACTION_HPP
