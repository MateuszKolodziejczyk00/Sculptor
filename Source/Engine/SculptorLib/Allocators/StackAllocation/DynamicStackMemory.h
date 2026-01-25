#pragma once

#include "SculptorAliases.h"
#include "ProfilerCore.h"
#include "MathUtils.h"
#include "Utility/NonCopyable.h"


namespace spt::lib
{

class StackMemoryChunksAllocator
{
public:

	Byte* Allocate(SizeType size)
	{
		return new Byte[size];
	}

	void Deallocate(Byte* ptr, SizeType size)
	{
		delete[] ptr;
	}
};


template<typename ChunksAllocator = StackMemoryChunksAllocator>
class DynamicStackMemory : public NonCopyable
{
public:

	explicit DynamicStackMemory(SizeType memoryChunkSize)
		: m_memoryChunkSize(memoryChunkSize)
		, m_firstChunk(nullptr)
		, m_lastChunk(nullptr)
		, m_currentChunkOffset(0)
	{ }

	~DynamicStackMemory()
	{
		Reset();
	}

	void* Allocate(SizeType size, SizeType alignment)
	{
		SPT_CHECK(size <= m_memoryChunkSize);
	
		if (!m_lastChunk)
		{
			AllocNewChunk();
		}
	
		void* allocatedFromCurrentChunk = TryAllocFromCurrentChunk(size, alignment);
	
		if (!allocatedFromCurrentChunk)
		{
			AllocNewChunk();
			allocatedFromCurrentChunk = TryAllocFromCurrentChunk(size, alignment);
		}
	
		SPT_CHECK(!!allocatedFromCurrentChunk);
		return allocatedFromCurrentChunk;
	}

	void Deallocate()
	{
		SPT_PROFILER_FUNCTION();
	
		Reset();
	}

private:

	struct MemChunk
	{
		MemChunk()
			: next(nullptr)
		{ }
		MemChunk*	next;
	};

	MemChunk* AllocNewChunk()
	{
		SPT_PROFILER_FUNCTION();
	
		m_currentChunkOffset = sizeof(MemChunk);
	
		Byte* memory = m_chunksAllocator.Allocate(m_memoryChunkSize);
		MemChunk* newChunk = new (memory) MemChunk();
	
		if (m_firstChunk)
		{
			m_lastChunk->next = newChunk;
		}
		else
		{
			m_firstChunk = newChunk;
		}
	
		m_lastChunk = newChunk;
		return newChunk;
	}

	void* TryAllocFromCurrentChunk(SizeType size, SizeType alignment)
	{
		const SizeType chunkAddress = reinterpret_cast<SizeType>(m_lastChunk);
		const SizeType allocationAddress = math::Utils::RoundUp(chunkAddress + m_currentChunkOffset, alignment);
	
		if (chunkAddress + m_memoryChunkSize - allocationAddress >= size)
		{
			m_currentChunkOffset = allocationAddress + size - chunkAddress;
			return reinterpret_cast<void*>(allocationAddress);
		}
		else
		{
			return nullptr;
		}
	}

	void Reset()
	{
		while (m_firstChunk)
		{
			MemChunk* next = m_firstChunk->next;
	
			m_firstChunk->~MemChunk();
			
			m_chunksAllocator.Deallocate(reinterpret_cast<Byte*>(m_firstChunk), m_memoryChunkSize);
	
			m_firstChunk = next;
		}

		m_lastChunk = nullptr;
		m_currentChunkOffset = 0;
	}

	SizeType m_memoryChunkSize;

	ChunksAllocator m_chunksAllocator;

	MemChunk* m_firstChunk;
	MemChunk* m_lastChunk;

	SizeType m_currentChunkOffset;
};

} // spt::lib
