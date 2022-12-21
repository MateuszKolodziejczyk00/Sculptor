#include "RGAllocator.h"


namespace spt::rg
{

class RGMemoryChunksPool
{
public:

	static RGMemoryChunksPool& Get()
	{
		static RGMemoryChunksPool instance;
		return instance;
	}

	Byte* AcquireMemory()
	{
		{
			const lib::LockGuard guard(m_lock);

			if (!m_memoryPool.empty())
			{
				Byte* memoryPtr = m_memoryPool.back();
				m_memoryPool.pop_back();

				return memoryPtr;
			}
		}

		return new Byte[RenderGraphMemoryChunkSize];
	}

	void ReleaseMemory(Byte* memory)
	{
		const lib::LockGuard guard(m_lock);
		
		m_memoryPool.emplace_back(memory);
	}

private:
	
	RGMemoryChunksPool() = default;

	lib::DynamicArray<Byte*> m_memoryPool;
	lib::Lock m_lock;
};

Byte* RGStackMemoryChunksAllocator::Allocate(SizeType size)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(size == RenderGraphMemoryChunkSize);
	return RGMemoryChunksPool::Get().AcquireMemory();
}

void RGStackMemoryChunksAllocator::Deallocate(Byte* ptr, SizeType size)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(size == RenderGraphMemoryChunkSize);
	RGMemoryChunksPool::Get().ReleaseMemory(ptr);
}

} // spt::rg
