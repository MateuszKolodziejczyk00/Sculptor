#pragma once

#include "SculptorAliases.h"
#include "Assertions/Assertions.h"
#include <utility>


namespace spt::lib
{

template <typename T, SizeType maxSize>
class InlineArray
{
public:

	using ValueType = T;

	InlineArray()
		: m_size(0)
	{}

	SizeType GetSize() const { return m_size; }

	bool IsEmpty() const { return m_size == 0; }

	bool IsFull() const { return m_size == maxSize; }

	static constexpr SizeType GetCapacity() { return maxSize; }

	void PushBack(const ValueType& value)
	{
		SPT_CHECK(m_size < maxSize);
		m_inlineData[m_size++] = value;
	}

	void PushBack(ValueType&& value)
	{
		SPT_CHECK(m_size < maxSize);
		m_inlineData[m_size++] = std::move(value);
	}

	template<typename... Args>
	ValueType& EmplaceBack(Args&&... args)
	{
		SPT_CHECK(m_size < maxSize);
		m_inlineData[m_size] = ValueType(std::forward<Args>(args)...);
		return m_inlineData[m_size++];
	}

	void PopBack()
	{
		SPT_CHECK(m_size > 0);
		m_inlineData[m_size - 1].~ValueType();
		--m_size;
	}

	ValueType& operator[](SizeType index)
	{
		SPT_CHECK(index < m_size);
		return m_inlineData[index];
	}

	const ValueType& operator[](SizeType index) const
	{
		SPT_CHECK(index < m_size);
		return m_inlineData[index];
	}

	ValueType* begin() { return m_inlineData; }
	const ValueType* begin() const { return m_inlineData; }
	const ValueType* cbegin() const { return m_inlineData; }

	ValueType* end() { return m_inlineData + m_size; }
	const ValueType* end() const { return m_inlineData + m_size; }
	const ValueType* cend() const { return m_inlineData + m_size; }

	void Clear()
	{
		for (SizeType i = 0; i < m_size; ++i)
		{
			m_inlineData[i].~ValueType();
		}
		m_size = 0;
	}

	ValueType& Front()
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[0];
	}

	const ValueType& Front() const
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[0];
	}

	ValueType& Back()
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[m_size - 1];
	}

	const ValueType& Back() const
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[m_size - 1];
	}

	void RemoveAtSwap(SizeType index)
	{
		SPT_CHECK(index < m_size);
		m_inlineData[index] = std::move(m_inlineData[m_size - 1]);
		m_inlineData[m_size - 1].~ValueType();
		--m_size;
	}

	ValueType* GetData() { return m_inlineData; }
	const ValueType* GetData() const { return m_inlineData; }

	Bool Contains(const ValueType& value) const
	{
		for (SizeType i = 0; i < m_size; ++i)
		{
			if (m_inlineData[i] == value)
			{
				return true;
			}
		}
		return false;
	}

	void AddUnique(const ValueType& value)
	{
		if (!Contains(value))
		{
			PushBack(value);
		}
	}

private:
	
	SizeType m_size;
	ValueType m_inlineData[maxSize];
};

} // spt::lib
