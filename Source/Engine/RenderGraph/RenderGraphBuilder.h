#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "Pipelines/PipelineState.h"
#include "RGDescriptorSetState.h"
#include "RGResources/RGResources.h"
#include "RGResources/RGAllocator.h"


namespace spt::rg
{

template<typename... TDescriptorSetStates>
auto BindDescriptorSets(TDescriptorSetStates&&... descriptorSetStates)
{
	constexpr SizeType size = lib::ParameterPackSize<TDescriptorSetStates...>::Count;
	return lib::StaticArray<lib::SharedPtr<rg::RGDescriptorSetState>, size>{ descriptorSetStates... };
}


class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder();

	RGTextureHandle AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture);

	RGTextureHandle CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);

	template<typename TDescriptorSetStatesRange>
	void AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, TDescriptorSetStatesRange&& dsStatesRange);

private:

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;

	lib::DynamicArray<RGNode*> m_nodes;

	RGAllocator allocator;
};

template<typename TDescriptorSetStatesRange>
void rg::RenderGraphBuilder::AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY();
}

} // spt::rg
