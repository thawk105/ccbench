#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include "timeStamp.hpp"

class Message {
public:
	unsigned int transactionID;	//コミットされたトランザクションID
	TimeStamp commitWts;

	Message(unsigned int transactionID, TimeStamp wts) {
		this->transactionID = transactionID;
		commitWts = wts;
	}

};

#endif	//	MESSAGE_HPP
