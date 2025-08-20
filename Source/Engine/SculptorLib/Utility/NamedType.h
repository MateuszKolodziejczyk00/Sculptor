#pragma once

namespace spt::lib
{

template <typename TType, typename TParameter>
class NamedType
{
public:

	using DataType = TType;

	constexpr NamedType()
		: value{}
	{}

	constexpr NamedType(const NamedType& inValue)
		: value(inValue.value)
	{}

	constexpr NamedType(NamedType&& inValue)
		: value(std::move(inValue.value))
	{}

	constexpr explicit NamedType(const TType& inValue)
		: value(inValue)
	{}

	constexpr explicit NamedType(TType&& inValue)
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

	Bool operator==(const NamedType& inValue) const
	{
		return value == inValue.value;
	}

	Bool operator!=(const NamedType& inValue) const
	{
		return value != inValue.value;
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

protected:

	TType value;
};


template<typename TType, typename TParameterType>
struct Hasher<NamedType<TType, TParameterType>>
{
    size_t operator()(const NamedType<TType, TParameterType>& value) const
    {
		return GetHash(value.Get());
    }
};

} // spt::lib