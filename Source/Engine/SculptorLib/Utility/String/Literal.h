#pragma once


namespace spt::lib
{

namespace impl
{

// Reference: https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/
template<typename TType, SizeType length>
class Literal
{
public:

	constexpr Literal(const TType (&literal)[length])
	{
		std::copy_n(literal, length, m_literal);
	}

	constexpr const TType* Get() const
	{
		return m_literal;
	}

	// this struct must be a literal class type, so all its members must be public
	TType m_literal[length];
};

} // impl


template<SizeType length>
using Literal = impl::Literal<char, length>;


template<SizeType length>
using WLiteral = impl::Literal<wchar_t, length>;

} // spt::lib