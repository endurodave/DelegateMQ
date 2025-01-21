#include "UnitTestCommon.h"

namespace UnitTestData
{
	int Class::m_construtorCnt = 0;

	std::ostream& operator<<(std::ostream& os, const Class& data) {
		return os;
	}

	std::istream& operator>>(std::istream& is, Class& data) {
		return is;
	}
}