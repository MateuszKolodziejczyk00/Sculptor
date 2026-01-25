#pragma once

#include "DynamicStackMemory.h"
#include "Containers/DynamicArray.h"
#include "ProfilerCore.h"


namespace spt::lib
{

template<typename TTrackedType, typename ChunksAllocator = StackMemoryChunksAllocator>
class StackTrackingAllocator
{
public:

	explicit StackTrackingAllocator(SizeType memoryChunkSize = 1024 * 1024)
		: m_stackMemory(memoryChunkSize)
	{ }

	~StackTrackingAllocator()
	{
		Deallocate();
	}

	template<typename TType, typename... TArgs>
	TType* Allocate(TArgs&&... args)
	{
		static_assert(std::is_base_of_v<TTrackedType, TType>);

		TType* object = AllocateUntracked<TType>(std::forward<TArgs>(args)...);
		m_trackedObjects.emplace_back(object);
		return object;
	}

	template<typename TType, typename... TArgs>
	TType* AllocateUntracked(TArgs&&... args)
	{
		constexpr SizeType size			= sizeof(TType);
		constexpr SizeType alignment	= alignof(TType);

		void* memoryPtr = m_stackMemory.Allocate(size, alignment);

		return new (memoryPtr) TType(std::forward<TArgs>(args)...);
	}

	Byte* AllocateMemory(Uint32 size)
	{
		return reinterpret_cast<Byte*>(m_stackMemory.Allocate(size, 1u));
	}

	void Deallocate()
	{
		SPT_PROFILER_FUNCTION();

		for (TTrackedType* trackedObj : m_trackedObjects)
		{
			trackedObj->~TTrackedType();
		}

		m_trackedObjects.clear();
	}

private:

	using StackMemoryType = DynamicStackMemory<ChunksAllocator>;

	StackMemoryType m_stackMemory;

	lib::DynamicArray<TTrackedType*> m_trackedObjects;
};

} // spt::lib
