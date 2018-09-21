#pragma once

#define R_INTENS 80
#define RW_EVEN 50
#define W_INTENS 20

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
