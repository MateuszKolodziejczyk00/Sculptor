#include "RenderingDataRegistry.h"

namespace spt::rsc
{

RenderingDataRegistry& RenderingData::Get()
{
	return GetInstance().m_registry;
}

RenderingData& RenderingData::GetInstance()
{
	static RenderingData instance;
	return instance;
}

} // spt::rsc