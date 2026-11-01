#pragma once

#include "SculptorAliases.h"
#include "Allocators/MemoryArena.h"
#include "Assertions/Assertions.h"
#include <type_traits>
#include <utility>


namespace spt::lib
{

template<typename TType, typename TMemoryArena = MemoryArena>
class ArenaArray
{
public:

	using value_type = TType;
	using reference  = TType&;

	ArenaArray(TMemoryArena& arena, Uint32 maxSize)
		: m_data(reinterpret_cast<TType*>(arena.Allocate(sizeof(TType) * maxSize, alignof(TType))))
		, m_maxSize(maxSize)
	{ }

	~ArenaArray()
	{
		Reset();
	}

	ArenaArray(const ArenaArray&) = delete;
	ArenaArray& operator=(const ArenaArray&) = delete;

	template<typename... TArgs>
	TType& EmplaceBack(TArgs&&... args)
	{
		SPT_CHECK(m_size < m_maxSize);
		TType* elementPtr = new (m_data + m_size) TType(std::forward<TArgs>(args)...);
		++m_size;
		return *elementPtr;
	}

	void PushBack(const TType& value) { EmplaceBack(value); }
	void PushBack(TType&& value)      { EmplaceBack(std::move(value)); }

	void PopBack()
	{
		SPT_CHECK(!IsEmpty());
		--m_size;
		m_data[m_size].~TType();
	}

	void RemoveAt(Uint32 index)
	{
		SPT_CHECK(index < m_size);
		for (Uint32 i = index; i + 1u < m_size; ++i)
		{
			m_data[i] = std::move(m_data[i + 1u]);
		}
		PopBack();
	}

	void Reset()
	{
		if constexpr (!std::is_trivially_destructible_v<TType>)
		{
			for (Uint32 i = 0u; i < m_size; ++i)
			{
				m_data[i].~TType();
			}
		}
		m_size = 0u;
	}

	TType& operator[](Uint32 index)
	{
		SPT_CHECK(index < m_size);
		return m_data[index];
	}

	const TType& operator[](Uint32 index) const
	{
		SPT_CHECK(index < m_size);
		return m_data[index];
	}

	TType& Back()             { return (*this)[m_size - 1u]; }
	const TType& Back() const { return (*this)[m_size - 1u]; }

	Uint32 GetSize() const    { return m_size; }
	Uint32 GetMaxSize() const { return m_maxSize; }
	Bool   IsEmpty() const    { return m_size == 0u; }
	Bool   IsFull() const     { return m_size == m_maxSize; }

	TType*       GetData()       { return m_data; }
	const TType* GetData() const { return m_data; }

	TType*       begin()       { return m_data; }
	const TType* begin() const { return m_data; }
	TType*       end()         { return m_data + m_size; }
	const TType* end() const   { return m_data + m_size; }

private:

	TType* m_data    = nullptr;
	Uint32 m_size    = 0u;
	Uint32 m_maxSize = 0u;
};

} // spt::lib
