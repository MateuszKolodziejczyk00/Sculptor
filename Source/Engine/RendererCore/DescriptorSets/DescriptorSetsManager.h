#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "PersistentDescriptorSetsManager.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class RenderContext;
class Pipeline;


class RENDERER_CORE_API DescriptorSetsManager
{
public:

	DescriptorSetsManager();

	void BeginFrame();

	rhi::RHIDescriptorSet GetDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& descriptorSetState) const;
	rhi::RHIDescriptorSet GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::MTHandle<DescriptorSetState>& descriptorSetState);

	void UnregisterDescriptorSet(const DescriptorSetState& descriptorSetState);

private:

	PersistentDescriptorSetsManager m_persistentDescriptorSets;
};

} // spt::rdr