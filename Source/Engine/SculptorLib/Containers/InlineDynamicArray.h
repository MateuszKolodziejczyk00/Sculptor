#pragma once

#include "SculptorAliases.h"
#include "Assertions/Assertions.h"
#include "Utility/Templates/TypeStorage.h"

#include <initializer_list>
#include <type_traits>
#include <utility>


namespace spt::lib
{

template <typename T, SizeType maxSize>
class InlineDynamicArray
{
public:

	using ValueType        = T;
	using value_type       = ValueType;
	using reference        = ValueType&;
	using const_reference  = const ValueType&;
	using pointer          = ValueType*;
	using const_pointer    = const ValueType*;
	using iterator         = ValueType*;
	using const_iterator   = const ValueType*;
	using size_type        = SizeType;
	using difference_type  = std::ptrdiff_t;

	static constexpr SizeType s_storageCapacity = maxSize > 0u ? maxSize : 1u;

	InlineDynamicArray() = default;

	InlineDynamicArray(std::initializer_list<ValueType> initializerList)
	{
		SPT_CHECK(initializerList.size() <= maxSize);

		for (const ValueType& value : initializerList)
		{
			PushBack(value);
		}
	}

	InlineDynamicArray(const InlineDynamicArray& other)
	{
		CopyFrom(other);
	}

	InlineDynamicArray(InlineDynamicArray&& other) noexcept(std::is_nothrow_move_constructible_v<ValueType>)
	{
		MoveFrom(std::move(other));
	}

	~InlineDynamicArray()
	{
		Clear();
	}

	InlineDynamicArray& operator=(const InlineDynamicArray& other)
	{
		if (this != &other)
		{
			Clear();
			CopyFrom(other);
		}

		return *this;
	}

	InlineDynamicArray& operator=(InlineDynamicArray&& other) noexcept(std::is_nothrow_move_constructible_v<ValueType>)
	{
		if (this != &other)
		{
			Clear();
			MoveFrom(std::move(other));
		}

		return *this;
	}

	SizeType GetSize() const { return m_size; }
	SizeType size() const { return m_size; }

	Bool IsEmpty() const { return m_size == 0u; }
	Bool empty() const { return IsEmpty(); }

	Bool IsFull() const { return m_size == maxSize; }

	static constexpr SizeType GetCapacity() { return maxSize; }
	static constexpr SizeType capacity() { return maxSize; }
	static constexpr SizeType max_size() { return maxSize; }

	void PushBack(const ValueType& value)
	{
		EmplaceBack(value);
	}

	void PushBack(ValueType&& value)
	{
		EmplaceBack(std::move(value));
	}

	template<typename... Args>
	ValueType& EmplaceBack(Args&&... args)
	{
		SPT_CHECK(m_size < maxSize);

		m_inlineData[m_size].Construct(std::forward<Args>(args)...);
		++m_size;

		return m_inlineData[m_size - 1u].Get();
	}

	void PopBack()
	{
		SPT_CHECK(!IsEmpty());

		--m_size;
		m_inlineData[m_size].Destroy();
	}

	reference operator[](SizeType index)
	{
		SPT_CHECK(index < m_size);
		return m_inlineData[index].Get();
	}

	const_reference operator[](SizeType index) const
	{
		SPT_CHECK(index < m_size);
		return m_inlineData[index].Get();
	}

	pointer GetData() { return m_inlineData[0].GetAddress(); }
	const_pointer GetData() const { return m_inlineData[0].GetAddress(); }
	pointer data() { return GetData(); }
	const_pointer data() const { return GetData(); }

	iterator begin() { return GetData(); }
	const_iterator begin() const { return GetData(); }
	const_iterator cbegin() const { return GetData(); }

	iterator end() { return GetData() + m_size; }
	const_iterator end() const { return GetData() + m_size; }
	const_iterator cend() const { return GetData() + m_size; }

	void Clear()
	{
		while (!IsEmpty())
		{
			PopBack();
		}
	}

	reference Front()
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[0].Get();
	}

	const_reference Front() const
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[0].Get();
	}

	reference Back()
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[m_size - 1u].Get();
	}

	const_reference Back() const
	{
		SPT_CHECK(!IsEmpty());
		return m_inlineData[m_size - 1u].Get();
	}

	void RemoveAtSwap(SizeType index)
	{
		SPT_CHECK(index < m_size);

		const SizeType lastIndex = m_size - 1u;
		if (index != lastIndex)
		{
			if constexpr (std::is_move_assignable_v<ValueType>)
			{
				m_inlineData[index].Get() = std::move(m_inlineData[lastIndex].Get());
			}
			else
			{
				m_inlineData[index].Destroy();
				m_inlineData[index].Construct(std::move(m_inlineData[lastIndex].Get()));
			}
		}

		m_inlineData[lastIndex].Destroy();
		--m_size;
	}

	void RemoveAt(SizeType index)
	{
		SPT_CHECK(index < m_size);

		for (SizeType i = index; i < m_size - 1u; ++i)
		{
			if constexpr (std::is_move_assignable_v<ValueType>)
			{
				m_inlineData[i].Get() = std::move(m_inlineData[i + 1u].Get());
			}
			else
			{
				m_inlineData[i].Destroy();
				m_inlineData[i].Construct(std::move(m_inlineData[i + 1u].Get()));
			}
		}

		m_inlineData[m_size - 1u].Destroy();
		--m_size;
	}

	void RemoveElementSwap(const ValueType& value)
	{
		for (SizeType i = 0; i < m_size; ++i)
		{
			if (m_inlineData[i].Get() == value)
			{
				RemoveAtSwap(i);
				return;
			}
		}
	}

	void ResizeUninitialized(SizeType newSize)
	{
		SPT_CHECK(newSize <= maxSize);

		if (m_size < newSize)
		{
			m_size = newSize;
		}

		while (m_size > newSize)
		{
			--m_size;
			m_inlineData[m_size].Destroy();
		}
	}

	void Resize(SizeType newSize, const ValueType& defaultValue = ValueType())
	{
		SPT_CHECK(newSize <= maxSize);

		if (m_size < newSize)
		{
			while (m_size < newSize)
			{
				PushBack(defaultValue);
			}
		}
		else
		{
			while (m_size > newSize)
			{
				PopBack();
			}
		}
	}

	Bool Contains(const ValueType& value) const
	{
		for (SizeType i = 0; i < m_size; ++i)
		{
			if (m_inlineData[i].Get() == value)
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

	void CopyFrom(const InlineDynamicArray& other)
	{
		for (const ValueType& value : other)
		{
			PushBack(value);
		}
	}

	void MoveFrom(InlineDynamicArray&& other)
	{
		for (ValueType& value : other)
		{
			PushBack(std::move(value));
		}

		other.Clear();
	}

	SizeType m_size = 0u;
	TypeStorage<ValueType> m_inlineData[s_storageCapacity];
};

} // spt::lib
