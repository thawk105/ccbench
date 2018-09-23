#pragma once

enum class Workload {
	R_ONLY, R_INTENS, RW_EVEN, W_INTENS, W_ONLY
};

enum class Ope {
	READ,
	WRITE,
};

class Procedure {
public:
	Ope ope = Ope::READ;
	int key = 0;
	int val = 0;
};
