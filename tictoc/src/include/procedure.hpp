#pragma once

enum class Ope {
	READ, 
	WRITE
};

class Procedure {
public:
	Ope ope = Ope::READ;
	int key = 0;
	int val = 0;
};
