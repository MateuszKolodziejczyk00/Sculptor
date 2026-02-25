#pragma once

#include "NamedType.h"


namespace spt::lib
{

template <typename TType, typename TParameter>
class NamedHandle
{
public:

	using DataType = lib::NamedType<TType, TParameter>;
	static constexpr DataType s_none = DataType{idxNone<TType>};

	NamedHandle()
		: m_value{ s_none }
	{}

	NamedHandle(NamedHandle&& inValue)
		: m_value(std::move(inValue.m_value))
	{
		inValue.m_value = s_none;
	}

	explicit NamedHandle(const TType& inValue)
		: m_value(DataType(inValue))
	{}

	explicit NamedHandle(TType&& inValue)
		: m_value(DataType(std::move(inValue)))
	{}

	NamedHandle& operator=(NamedHandle&& inValue)
	{
		m_value = std::move(inValue.m_value);
		inValue.m_value = s_none;
		return *this;
	}

	~NamedHandle()
	{
		SPT_CHECK(!IsValid());
	}

	Bool IsValid() const
	{
		return m_value != s_none;
	}

	void Reset()
	{
		m_value = s_none;
	}

	DataType& Get()
	{
		return m_value;
	}

	const DataType& Get() const
	{
		return m_value;
	}

	operator DataType()
	{
		return m_value;
	}
	
	operator DataType() const
	{
		return m_value;
	}

protected:

	DataType m_value;
};

} // spt::lib
