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

	void Initialize();
	void Uninitialize();

	void BeginFrame();

	rhi::RHIDescriptorSet GetDescriptorSet(const lib::SharedRef<DescriptorSetState>& descriptorSetState) const;
	rhi::RHIDescriptorSet GetOrCreateDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, Uint32 descriptorSetIdx, const lib::SharedRef<DescriptorSetState>& descriptorSetState);

private:

	PersistentDescriptorSetsManager m_persistentDescriptorSets;
};

} // spt::rdr