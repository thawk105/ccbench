#pragma once

enum class Ope {
	READ, 
	WRITE
};

class Procedure {
public:
	Ope ope = Ope::READ;
	unsigned int key = 0;
	unsigned int val = 0;

	bool operator<(const Procedure& right) const {
		return this->key < right.key;
	}
};

