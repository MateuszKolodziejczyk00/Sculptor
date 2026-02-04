#pragma once

#include "SculptorLibMacros.h"
#include "SculptorAliases.h"
#include "Span.h"


namespace spt::lib
{

template<typename T>
class SCULPTOR_LIB_API ManagedSpan
{
public:

	ManagedSpan() = default;

	ManagedSpan(lib::Span<T> span)
		: m_span(span)
	{
		for (T& element : m_span)
		{
			new (&element) T();
		}
	}

	~ManagedSpan()
	{
		for (T& element : m_span)
		{
			element.~T();
		}
	}

	ManagedSpan(ManagedSpan&& other)
		: m_span(other.m_span)
	{
		other.m_span = lib::Span<T>();
	}

	ManagedSpan& operator=(ManagedSpan&& other)
	{
		if (this != &other)
		{
			for (T& element : m_span)
			{
				element.~T();
			}

			m_span = other.m_span;
			other.m_span = lib::Span<T>();
		}

		return *this;
	}

	Uint64 Size() const
	{
		return m_span.size();
	}

	T& operator[](Uint64 idx) const
	{
		SPT_CHECK(idx < Size());
		return m_span[idx];
	}

	lib::Span<T> ToSpan() const
	{
		return m_span;
	}

	operator lib::Span<T>() const
	{
		return m_span;
	}

	const auto begin() const { return m_span.begin(); }
	const auto end() const { return m_span.end(); }
	const auto cbegin() const { return m_span.cbegin(); }
	const auto cend() const { return m_span.cend(); }

	const T* GetData() const { return m_span.data(); }
	
private:

	lib::Span<T> m_span;
};

} // spt::lib
