#pragma once

#include "SculptorAliases.h"
#include "Allocators/MemoryArena.h"
#include <type_traits>


namespace spt::lib
{

template<typename TType, typename TMemoryArena = MemoryArena>
class DynamicPushArray
{
	struct ChunkHeader
	{
		ChunkHeader* next     = nullptr;
		Uint32       size     = 0u;
		Uint32       capacity = 0u;
	};

public:

	using reference = TType&;

	class Iterator
	{
	public:

		Iterator()
			: m_currentChunk(nullptr)
			, m_currentIdx(0u)
		{ }

		Iterator(ChunkHeader* chunk, Uint32 idx)
			: m_currentChunk(chunk)
			, m_currentIdx(idx)
		{ }

		TType& operator*() const
		{
			return reinterpret_cast<TType*>(m_currentChunk + 1)[m_currentIdx];
		}

		Iterator& operator++()
		{
			++m_currentIdx;
			if (m_currentIdx >= m_currentChunk->size)
			{
				m_currentChunk = m_currentChunk->next;
				m_currentIdx = 0u;
			}
			return *this;
		}

		Bool operator!=(const Iterator& other) const
		{
			return m_currentChunk != other.m_currentChunk || m_currentIdx != other.m_currentIdx;
		}

	private:

		ChunkHeader* m_currentChunk;
		Uint32       m_currentIdx;
	};

	DynamicPushArray(MemoryArena& arena)
		: m_arena(arena)
	{}

	~DynamicPushArray()
	{
		Reset();
	}

	Bool IsEmpty() const
	{
		return m_firstChunk == nullptr || m_firstChunk->size == 0u;
	}

	Iterator begin() const
	{
		return Iterator(m_firstChunk, 0u);
	}

	Iterator end() const
	{
		return Iterator(nullptr, 0u);
	}

	template<typename... TArgs>
	TType& EmplaceBack(TArgs&&... args)
	{
		if (!m_lastChunk || m_lastChunk->size >= m_lastChunk->capacity)
		{
			const Uint32 newCapacity = m_lastChunk ? m_lastChunk->capacity * 2u : 8u;
			ChunkHeader* newChunk = reinterpret_cast<ChunkHeader*>(m_arena.Allocate(sizeof(ChunkHeader) + sizeof(TType) * newCapacity, alignof(ChunkHeader)));
			newChunk->next = nullptr;
			newChunk->size = 0u;
			newChunk->capacity = newCapacity;

			if (m_lastChunk)
			{
				m_lastChunk->next = newChunk;
			}
			else
			{
				m_firstChunk = newChunk;
			}

			m_lastChunk = newChunk;
		}

		TType* elementPtr = reinterpret_cast<TType*>(m_lastChunk + 1) + m_lastChunk->size;
		new (elementPtr) TType(std::forward<TArgs>(args)...);
		++m_lastChunk->size;

		return *elementPtr;
	}

	void Reset()
	{
		if constexpr (!std::is_trivially_destructible_v<TType>)
		{
			Iterator it = begin();

			while (it != end())
			{
				(*it).~TType();
				++it;
			}
		}

		m_firstChunk = nullptr;
		m_lastChunk  = nullptr;
	}

private:

	TMemoryArena& m_arena;
	ChunkHeader*  m_firstChunk = nullptr;
	ChunkHeader*  m_lastChunk  = nullptr;
};

} // spt::lib
