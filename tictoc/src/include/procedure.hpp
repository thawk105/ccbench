#ifndef PROCEDURE_HPP
#define PROCEDURE_HPP

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

#endif	//	PROCEDURE_HPP
