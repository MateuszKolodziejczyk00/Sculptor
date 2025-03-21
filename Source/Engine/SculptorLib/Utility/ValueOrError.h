#pragma once

#include "SculptorAliases.h"

#include <variant>


namespace spt::lib
{

template<typename TValue, typename TError>
class ValueOrError
{
public:

	ValueOrError(const TValue& value)
		: m_data(value)
	{
	}

	ValueOrError(TValue&& value)
		: m_data(std::move(value))
	{
	}

	ValueOrError(const TError& error)
		: m_data(error)
	{
	}

	ValueOrError(TError&& error)
		: m_data(std::move(error))
	{
	}

	TValue& GetValue()
	{
		return std::get<TValue>(m_data);
	}

	const TValue& GetValue() const
	{
		return std::get<TValue>(m_data);
	}

	const TError& GetError() const
	{
		return std::get<TError>(m_data);
	}

	Bool HasValue() const
	{
		return m_data.index() == 0;
	}

	Bool HasError() const
	{
		return m_data.index() == 1;
	}

	operator Bool() const
	{
		return HasValue();
	}

private:

	std::variant<TValue, TError> m_data;
};

} // spt::lib