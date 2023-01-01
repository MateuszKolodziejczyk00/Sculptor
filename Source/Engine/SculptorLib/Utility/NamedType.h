#pragma once

namespace spt::lib
{

template <typename TType, typename TParameter>
class NamedType
{
public:

	using DataType = TType;

	NamedType()
		: value{}
	{}

	NamedType(const NamedType& inValue)
		: value(inValue.value)
	{}

	NamedType(NamedType&& inValue)
		: value(std::move(inValue.value))
	{}

	explicit NamedType(const TType& inValue)
		: value(inValue)
	{}

	explicit NamedType(TType&& inValue)
		: value(std::move(inValue))
	{}

	TType& Get()
	{
		return value;
	}

	const TType& Get() const
	{
		return value;
	}

	operator TType()
	{
		return value;
	}
	
	operator TType() const
	{
		return value;
	}

private:

	TType value;
};

} // spt::lib