#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rhicore
{

struct RHIInitializationInfo
{
	RHIInitializationInfo()
		: m_extensions(nullptr)
		, m_extensionsNum(0)
	{ }

	const char* const* m_extensions;
	Uint32 m_extensionsNum;
};

}
