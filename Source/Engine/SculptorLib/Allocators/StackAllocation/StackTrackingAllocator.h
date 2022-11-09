#pragma once

#include "DynamicStackMemory.h"
#include "Containers/DynamicArray.h"


namespace spt::lib
{

template<typename TType, SizeType memChunkSize = 1024 * 1024>
class StackTrackingAllocator
{
public:

	StackTrackingAllocator() = default;

	~StackTrackingAllocator()
	{
		Deallocate();
	}

	template<typename... TArgs>
	TType* Allocate(TArgs&&... args)
	{
		TType* object = AllocateUntracked(std::forward<TArgs>(args)...);
		m_trackedObjects.emplace_back(object);
		return object;
	}

	template<typename... TArgs>
	TType* AllocateUntracked(TArgs&&... args)
	{
		constexpr SizeType size			= sizeof(TType);
		constexpr SizeType alignment	= alignof(TType);

		void* memoryPtr = m_stackMemory.Allocate(size, alignment);

		return new (memoryPtr) TType(std::forward<TArgs>(args)...);
	}

	void Deallocate()
	{
		for (TType* trackedObj : m_trackedObjects)
		{
			trackedObj->~TType();
		}
	}

private:

	using StackMemoryType = DynamicStackMemory<memChunkSize>;

	StackMemoryType m_stackMemory;

	lib::DynamicArray<TType*> m_trackedObjects;
};

} // spt::lib