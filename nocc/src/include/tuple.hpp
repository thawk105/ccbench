#ifndef TUPLE_HPP
#define TUPLE_HPP

#include <atomic>

using namespace std;

class Tuple {
public: 
	std::atomic<int> key;
	std::atomic<int> val;
};

#endif	//	TUPLE_HPP
