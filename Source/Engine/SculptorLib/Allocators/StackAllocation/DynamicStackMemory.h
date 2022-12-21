#pragma once

#include "SculptorLibMacros.h"
#include "SculptorAliases.h"
#include "ProfilerCore.h"
#include "MathUtils.h"
#include "Utility/NonCopyable.h"


namespace spt::lib
{

class SCULPTOR_LIB_API DynamicStackMemory : public NonCopyable
{
public:

	explicit DynamicStackMemory(SizeType memoryChunkSize);

	~DynamicStackMemory();

	void* Allocate(SizeType size, SizeType alignment);

	void Deallocate();

private:

	struct MemChunk
	{
		MemChunk()
			: next(nullptr)
		{ }
		MemChunk*	next;
	};

	MemChunk* AllocNewChunk();

	void* TryAllocFromCurrentChunk(SizeType size, SizeType alignment);

	void Reset();

	SizeType m_memoryChunkSize;

	MemChunk* m_firstChunk;
	MemChunk* m_lastChunk;

	SizeType m_currentChunkOffset;
};

} // spt::lib