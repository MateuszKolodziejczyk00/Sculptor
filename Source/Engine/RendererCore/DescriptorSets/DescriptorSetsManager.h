#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "PersistentDescriptorSetsState.h"
#include "RHIBridge/RHIDescriptorSetImpl.h"


namespace spt::rdr
{

class RENDERER_CORE_API DescriptorSetsManager
{
public:

	DescriptorSetsManager();

	void Initialize();
	void Uninitialize();

	void BeginFrame();

	rhi::RHIDescriptorSet GetDescriptorSet(const lib::SharedPtr<Pipeline>& pipeline, const lib::SharedPtr<DescriptorSetState>& descriptorSetState, OUT Uint32& outDescriptorSetIdx);

private:

	Uint32 GetDescriptorSetIdxInPipeline(const lib::SharedPtr<Pipeline>& pipeline, const lib::SharedRef<DescriptorSetState>& state) const;

	PersistentDescriptorSetsState m_persistentDescriptorSets;
};

} // spt::rdr