#pragma once

#include "SculptorAliases.h"
#include "Assertions/Assertions.h"
#include "Allocators/MemoryArena.h"

#include <type_traits>
#include <utility>


namespace spt::lib
{

template<typename TType>
class StackArenaArray
{
public:

	using ValueType       = TType;
	using value_type      = ValueType;
	using reference       = ValueType&;
	using const_reference = const ValueType&;
	using pointer         = ValueType*;
	using const_pointer   = const ValueType*;
	using iterator        = ValueType*;
	using const_iterator  = const ValueType*;
	using size_type       = SizeType;

	explicit StackArenaArray(MemoryArena& arena, SizeType initialCapacity = 0u)
		: m_arena(arena)
	{
		if (initialCapacity > 0u)
		{
			Reserve(initialCapacity);
		}
	}

	StackArenaArray(const StackArenaArray&) = delete;
	StackArenaArray& operator=(const StackArenaArray&) = delete;

	StackArenaArray(StackArenaArray&&) = delete;
	StackArenaArray& operator=(StackArenaArray&&) = delete;

	~StackArenaArray()
	{
		Clear();
	}

	SizeType GetSize() const { return m_size; }
	SizeType size() const { return m_size; }

	SizeType GetCapacity() const { return m_capacity; }
	SizeType capacity() const { return m_capacity; }

	Bool IsEmpty() const { return m_size == 0u; }
	Bool empty() const { return IsEmpty(); }

	pointer GetData() { return m_data; }
	const_pointer GetData() const { return m_data; }
	pointer data() { return GetData(); }
	const_pointer data() const { return GetData(); }

	iterator begin() { return m_data; }
	const_iterator begin() const { return m_data; }
	const_iterator cbegin() const { return m_data; }

	iterator end() { return m_data + m_size; }
	const_iterator end() const { return m_data + m_size; }
	const_iterator cend() const { return m_data + m_size; }

	reference operator[](SizeType index)
	{
		SPT_CHECK(index < m_size);
		return m_data[index];
	}

	const_reference operator[](SizeType index) const
	{
		SPT_CHECK(index < m_size);
		return m_data[index];
	}

	void Reserve(SizeType newCapacity)
	{
		if (newCapacity > m_capacity)
		{
			Grow(newCapacity);
		}
	}

	template<typename... TArgs>
	reference EmplaceBack(TArgs&&... args)
	{
		EnsureCapacity(m_size + 1u);

		TType* const elementPtr = m_data + m_size;
		new (elementPtr) TType(std::forward<TArgs>(args)...);
		++m_size;

		return *elementPtr;
	}

	void Clear()
	{
		DestroyElements(m_data, m_size);
		m_size = 0u;
	}

private:

	static constexpr SizeType GetNextCapacity(SizeType currentCapacity)
	{
		return currentCapacity > 0u ? currentCapacity * 2u : 8u;
	}

	static constexpr Uint64 GetStorageSize(SizeType capacity)
	{
		return static_cast<Uint64>(sizeof(TType)) * capacity;
	}

	void EnsureCapacity(SizeType requiredCapacity)
	{
		if (requiredCapacity > m_capacity)
		{
			SizeType newCapacity = GetNextCapacity(m_capacity);
			if (newCapacity < requiredCapacity)
			{
				newCapacity = requiredCapacity;
			}

			Grow(newCapacity);
		}
	}

	void Grow(SizeType newCapacity)
	{
		SPT_CHECK(newCapacity > m_capacity);

		const Uint64 currentStorageSize = GetStorageSize(m_capacity);
		const Uint64 newStorageSize     = GetStorageSize(newCapacity);

		if (m_data && m_arena.IsLastAllocation(m_data, currentStorageSize))
		{
			m_arena.GrowLastAllocation(m_data, currentStorageSize, newStorageSize);
		}
		else
		{
			TType* const newData = reinterpret_cast<TType*>(m_arena.Allocate(newStorageSize, alignof(TType)));
			MoveElements(newData);
			m_data = newData;
		}

		m_capacity = newCapacity;
	}

	void MoveElements(TType* destination)
	{
		for (SizeType idx = 0u; idx < m_size; ++idx)
		{
			new (destination + idx) TType(std::move(m_data[idx]));
		}

		DestroyElements(m_data, m_size);
	}

	void DestroyElements(TType* elements, SizeType elementsNum)
	{
		if constexpr (!std::is_trivially_destructible_v<TType>)
		{
			for (SizeType idx = elementsNum; idx > 0u; --idx)
			{
				elements[idx - 1u].~TType();
			}
		}
	}

	MemoryArena& m_arena;
	TType*       m_data     = nullptr;
	SizeType     m_size     = 0u;
	SizeType     m_capacity = 0u;
};

} // spt::lib
