#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "Pipelines/PipelineState.h"
#include "RGDescriptorSetState.h"
#include "RGResources/RGResources.h"
#include "RGResources/RGAllocator.h"
#include "DependenciesBuilder.h"
#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"

namespace spt::rhi
{
struct BarrierTextureTransitionTarget;
} // spt::rhi


namespace spt::rg
{

template<typename... TDescriptorSetStates>
auto BindDescriptorSets(TDescriptorSetStates&&... descriptorSetStates)
{
	constexpr SizeType size = lib::ParameterPackSize<TDescriptorSetStates...>::Count;
	return lib::StaticArray<lib::SharedPtr<rg::RGDescriptorSetStateBase>, size>{ descriptorSetStates... };
}


class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder();

	RGTextureHandle AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture);

	RGTextureHandle CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);

	template<typename TDescriptorSetStatesRange>
	void AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

private:

	template<typename TDescriptorSetStatesRange>
	void BuildDescriptorSetDependencies(TDescriptorSetStatesRange&& dsStatesRange, RGDependeciesContainer& dependencies);

	void AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& yependencies);

	const rhi::BarrierTextureTransitionTarget& GetTransitionDefForAccess(ERGAccess access) const;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;

	lib::DynamicArray<RGNodeHandle> m_nodes;

	RGAllocator allocator;
};

template<typename TDescriptorSetStatesRange>
void rg::RenderGraphBuilder::AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, groupCount](const lib::SharedPtr<rdr::CommandRecorder>& recorder)
	{
		recorder->BindComputePipeline(computePipelineID);
		recorder->Dispatch(groupCount);
	};

	using LambdaType = decltype(executeLambda);
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType* node = allocator.Allocate<NodeType>(std::move(executeLambda));

	RGDependeciesContainer dependencies;
	BuildDescriptorSetDependencies(dsStatesRange, dependencies);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::BuildDescriptorSetDependencies(TDescriptorSetStatesRange&& dsStatesRange, RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();

	RGDependenciesBuilder dependenciesBuilder(dependencies);
	for (const lib::SharedPtr<RGDescriptorSetStateBase>& stateToBind : dsStatesRange)
	{
		stateToBind->BuildRGDependencies(dependenciesBuilder);
	}
}

} // spt::rg
