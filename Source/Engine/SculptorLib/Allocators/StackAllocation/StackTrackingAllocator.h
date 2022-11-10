#pragma once

#include "DynamicStackMemory.h"
#include "Containers/DynamicArray.h"


namespace spt::lib
{

template<typename TTrackedType, SizeType memChunkSize = 1024 * 1024>
class StackTrackingAllocator
{
public:

	StackTrackingAllocator() = default;

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
		static_assert(std::is_base_of_v<TTrackedType, TType>);

		constexpr SizeType size			= sizeof(TTrackedType);
		constexpr SizeType alignment	= alignof(TTrackedType);

		void* memoryPtr = m_stackMemory.Allocate(size, alignment);

		return new (memoryPtr) TType(std::forward<TArgs>(args)...);
	}

	void Deallocate()
	{
		for (TTrackedType* trackedObj : m_trackedObjects)
		{
			trackedObj->~TTrackedType();
		}

		m_trackedObjects.clear();
	}

private:

	using StackMemoryType = DynamicStackMemory<memChunkSize>;

	StackMemoryType m_stackMemory;

	lib::DynamicArray<TTrackedType*> m_trackedObjects;
};

} // spt::lib