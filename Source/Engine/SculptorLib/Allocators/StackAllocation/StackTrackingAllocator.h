#pragma once

#include "DynamicStackMemory.h"
#include "Containers/DynamicArray.h"
#include "ProfilerCore.h"


namespace spt::lib
{

template<typename TTrackedType>
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
		TType* object = AllocateUntracked<TType>(std::forward<TArgs>(args)...);
		m_trackedObjects.emplace_back(object);
		return object;
	}

	template<typename TType, typename... TArgs>
	TType* AllocateUntracked(TArgs&&... args)
	{
		SPT_PROFILER_FUNCTION();

		static_assert(std::is_base_of_v<TTrackedType, TType>);

		constexpr SizeType size			= sizeof(TType);
		constexpr SizeType alignment	= alignof(TType);

		void* memoryPtr = m_stackMemory.Allocate(size, alignment);

		return new (memoryPtr) TType(std::forward<TArgs>(args)...);
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

	using StackMemoryType = DynamicStackMemory;

	StackMemoryType m_stackMemory;

	lib::DynamicArray<TTrackedType*> m_trackedObjects;
};

} // spt::lib