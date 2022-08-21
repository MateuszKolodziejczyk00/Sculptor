#pragma once

#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

struct CommandBufferAcquireInfo
{
	CommandBufferAcquireInfo()
		: commandPoolId(idxNone<SizeType>)
		, poolsSetHash(0)
	{ }

	void Reset()
	{
		commandPoolId = idxNone<SizeType>;
		poolsSetHash	= 0;
	}

	SizeType					commandPoolId;
	Uint32						poolsSetHash;
};

} // spt::vulkan
