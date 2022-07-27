#pragma once

#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

struct CommandBufferAcquireInfo
{
	CommandBufferAcquireInfo()
		: m_commandPoolId(idxNone<SizeType>)
		, m_poolsSetHash(0)
	{ }

	SizeType					m_commandPoolId;
	Uint32						m_poolsSetHash;
};

}
