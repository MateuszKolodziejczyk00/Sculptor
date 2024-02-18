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

	NamedType& operator=(const NamedType& inValue)
	{
		value = inValue.value;
		return *this;
	}

	NamedType& operator=(NamedType&& inValue)
	{
		value = std::move(inValue.value);
		return *this;
	}

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


namespace std
{
template<typename TType, typename TParameterType>
struct hash<spt::lib::NamedType<TType, TParameterType>>
{
    size_t operator()(const spt::lib::NamedType<TType, TParameterType>& value) const
    {
		return spt::lib::GetHash(value.Get());
    }
};

} // std