#pragma once
#include <cstdint>

#define R_INTENS 80
#define RW_EVEN 50
#define W_INTENS 20 

enum class Ope {
	READ, 
	WRITE
};

class Procedure {
public:
	unsigned int key = 0;
	unsigned int val = 0;
	Ope ope = Ope::READ;
	bool ronly = false;
	uint8_t pad[3];
};
