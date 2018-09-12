#include <iostream>
#include <bitset>
#include <unistd.h>
#include "timestamp.hpp"

int main() {
	TimeStamp ts;
	ts.generateTimeStampFirst(2);

	cout << ts.get_ts() << endl;
	cout << static_cast<bitset<64> >(ts.get_ts()) << endl;
	
	for (int i = 0; i < 5; i++) {
		ts.generateTimeStamp(2);
		cout << ts.get_ts() << endl;
		cout << static_cast<bitset<64> >(ts.get_ts()) << endl;
		sleep(1);
	}

	return 0;
}

