#pragma once

#include "SculptorAliases.h"
#include "ProfilerCore.h"
#include "MathUtils.h"
#include "Utility/NonCopyable.h"


namespace spt::lib
{

template<SizeType chunkSize>
class DynamicStackMemory : public  NonCopyable
{
public:

	DynamicStackMemory()
		: m_firstChunk(nullptr)
		, m_lastChunk(nullptr)
		, m_currentChunkOffset(0)
	{ }

	~DynamicStackMemory()
	{
		Reset();
	}

	void* Allocate(SizeType size, SizeType alignment)
	{
		SPT_CHECK(size <= chunkSize);

		if (m_lastChunk)
		{
			AllocNewChunk();
		}

		void* allocatedFromCurrentChunk = TryAllocFromCurrentChunk(size, alignment);

		if (allocatedFromCurrentChunk == nullptr)
		{
			AllocNewChunk();
			allocatedFromCurrentChunk = TryAllocFromCurrentChunk(size, alignment);
		}

		SPT_CHECK(!!allocatedFromCurrentChunk);
		return allocatedFromCurrentChunk;
	}

	void Deallocate()
	{
		Reset();
	}

private:

	struct MemChunk
	{
		MemChunk()
			: next(nullptr)
		{ }
		MemChunk*	next;
		Byte		memory[chunkSize];
	};

	MemChunk* AllocNewChunk()
	{
		SPT_PROFILER_FUNCTION();

		m_currentChunkOffset = 0;
		MemChunk* newChunk = new MemChunk();
		m_lastChunk->next = newChunk;
		m_lastChunk = newChunk;
		return newChunk;
	}

	void* TryAllocFromCurrentChunk(SizeType size, SizeType alignment)
	{
		const SizeType chunkAddress = reinterpret_cast<SizeType>(m_lastChunk->memory);
		const SizeType allocationAddress = math::Utils::RoundUp(chunkAddress + m_currentChunkOffset, alignment);

		if (chunkAddress + chunkSize - allocationAddress >= size)
		{
			m_currentChunkOffset = allocationAddress + size;
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

			delete m_firstChunk;

			m_firstChunk = next;
		}

		m_lastChunk = nullptr;
		m_currentChunkOffset = 0;
	}

	MemChunk* m_firstChunk;
	MemChunk* m_lastChunk;

	SizeType m_currentChunkOffset;
};

} // spt::lib