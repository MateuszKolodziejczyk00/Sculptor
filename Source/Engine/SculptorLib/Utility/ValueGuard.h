#pragma once

#include "Templates/TypeTraits.h"


namespace spt::lib
{

template<typename TType>
class ValueGuard
{
public:

	ValueGuard(TType& variable, AsConstParameter<TType> newValue)
		: m_variable(variable)
	{
		m_valueToRestore = m_variable;
		m_variable = newValue;
	}

	~ValueGuard()
	{
		m_variable = m_valueToRestore;
	}
	
private:

	TType& m_variable;

	TType m_valueToRestore;
};

} // spt::lib