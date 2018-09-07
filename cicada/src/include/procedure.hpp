#ifndef PROCEDURE_HPP
#define PROCEDURE_HPP

#include <cstdint>

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

#endif	//	PROCEDURE_HPP
