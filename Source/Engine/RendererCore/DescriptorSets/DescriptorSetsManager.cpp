#include "DescriptorSetsManager.h"
#include "Types/Pipeline/Pipeline.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"

namespace spt::rdr
{

DescriptorSetsManager::DescriptorSetsManager()
{ }

void DescriptorSetsManager::Initialize()
{
	rhi::RHIDescriptorSetManager::GetInstance().InitializeRHI();
}

void DescriptorSetsManager::Uninitialize()
{
	rhi::RHIDescriptorSetManager::GetInstance().ReleaseRHI();
}

void DescriptorSetsManager::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	m_persistentDescriptorSets.UpdatePersistentDescriptors();
}

rhi::RHIDescriptorSet DescriptorSetsManager::GetDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::SharedRef<DescriptorSetState>& descriptorSetState) const
{
	SPT_PROFILER_FUNCTION();

	// Descriptors without "Persistent" flag, should be accessed using render context
	SPT_CHECK(lib::HasAnyFlag(descriptorSetState->GetFlags(), EDescriptorSetStateFlags::Persistent));
	
	return m_persistentDescriptorSets.GetDescriptorSet(pipeline, descriptorSetIdx, descriptorSetState);
}

rhi::RHIDescriptorSet DescriptorSetsManager::GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::SharedRef<DescriptorSetState>& descriptorSetState)
{
	SPT_PROFILER_FUNCTION();

	// Descriptors without "Persistent" flag, should be created using render context
	SPT_CHECK(lib::HasAnyFlag(descriptorSetState->GetFlags(), EDescriptorSetStateFlags::Persistent));
	SPT_CHECK(descriptorSetIdx != idxNone<Uint32>);

	return m_persistentDescriptorSets.GetOrCreateDescriptorSet(pipeline, descriptorSetIdx, descriptorSetState);
}

} // spt::rdr
