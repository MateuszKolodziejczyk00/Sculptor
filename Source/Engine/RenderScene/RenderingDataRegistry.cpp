#include "RenderingDataRegistry.h"

namespace spt::rsc
{

RenderingDataRegistry& RenderingData::Get()
{
	return GetInstance().m_registry;
}

RenderingDataEntityHandle RenderingData::CreateEntity()
{
	RenderingDataRegistry& renderingDataRegistry = GetInstance().m_registry;
	const RenderingDataEntity staticMeshEntity = renderingDataRegistry.create();
	return RenderingDataEntityHandle(renderingDataRegistry, staticMeshEntity);
}

RenderingData& RenderingData::GetInstance()
{
	static RenderingData instance;
	return instance;
}

} // spt::rsc