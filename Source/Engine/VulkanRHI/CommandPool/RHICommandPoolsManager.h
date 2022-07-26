#pragma once

#include "RHICommandPool.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class QueueCommandPoolsManager
{
public:

	

private:

	lib::DynamicArray<RHICommandPool>		m_commandPools;

	lib::DynamicArray<SizeType>				m_freeCommandPools;
	lib::DynamicArray<SizeType>				m_lockedCommandPools;
};


class RHICommandPoolsManager
{
public:

	RHICommandPoolsManager();

private:

};

}
