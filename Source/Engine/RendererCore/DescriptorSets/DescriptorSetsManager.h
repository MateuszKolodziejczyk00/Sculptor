#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "PersistentDescriptorSetsState.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class Context;
class Pipeline;


class RENDERER_CORE_API DescriptorSetsManager
{
public:

	DescriptorSetsManager();

	void Initialize();
	void Uninitialize();

	void BeginFrame();

	rhi::RHIDescriptorSet GetDescriptorSet(const lib::SharedRef<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& descriptorSetState, Uint32 descriptorSetIdx);

private:

	PersistentDescriptorSetsState m_persistentDescriptorSets;
};

} // spt::rdr