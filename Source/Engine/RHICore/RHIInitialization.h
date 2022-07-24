#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rhi
{

struct RHIInitializationInfo
{
	RHIInitializationInfo()
		: m_extensions(nullptr)
		, m_extensionsNum(0)
	{ }

	const char* const*		m_extensions;
	Uint32					m_extensionsNum;
};

struct RHIWindowInitializationInfo
{
	RHIWindowInitializationInfo()
		: m_framebufferSize(idxNone<Uint32>, idxNone<Uint32>)
		, m_minImageCount(2)
	{ }

	math::Vector2u			m_framebufferSize;
	Uint32					m_minImageCount;
};

}
