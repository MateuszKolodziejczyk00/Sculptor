#include "DynamicStackMemory.h"
#include "SculptorCoreTypes.h"


namespace spt::lib
{

DynamicStackMemory::DynamicStackMemory(SizeType memoryChunkSize)
	: m_memoryChunkSize(memoryChunkSize)
	, m_firstChunk(nullptr)
	, m_lastChunk(nullptr)
	, m_currentChunkOffset(0)
{ }

DynamicStackMemory::~DynamicStackMemory()
{
	Reset();
}

void* DynamicStackMemory::Allocate(SizeType size, SizeType alignment)
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

void DynamicStackMemory::Deallocate()
{
	SPT_PROFILER_FUNCTION();

	Reset();
}

lib::DynamicStackMemory::MemChunk* DynamicStackMemory::AllocNewChunk()
{
	SPT_PROFILER_FUNCTION();

	m_currentChunkOffset = sizeof(MemChunk);

	Byte* memory = new Byte[m_memoryChunkSize];
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

void* DynamicStackMemory::TryAllocFromCurrentChunk(SizeType size, SizeType alignment)
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

void DynamicStackMemory::Reset()
{
	while (m_firstChunk)
	{
		MemChunk* next = m_firstChunk->next;

		m_firstChunk->~MemChunk();
		delete[] reinterpret_cast<Byte*>(m_firstChunk);

		m_firstChunk = next;
	}

	m_lastChunk = nullptr;
	m_currentChunkOffset = 0;
}

} // spt::lib