#ifndef TRANSACTION_HPP
#define TRANSACTION_HPP

#include "tsc.hpp"

class Transaction {
public:
	int tread(int key);
	void twrite(int key, int val);
};

#endif	//	TRANSACTION_HPP
