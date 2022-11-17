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

	const rhi::BarrierTextureTransitionTarget& GetTransitionDefForAccess(ERGAccess access) const;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;

	lib::DynamicArray<RGNode*> m_nodes;

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
	RGDependenciesBuilder dependenciesBuilder(dependencies);
	for (const lib::SharedPtr<RGDescriptorSetStateBase>& stateToBind : dsStatesRange)
	{
		stateToBind->BuildRGDependencies(dependenciesBuilder);
	}

	for (const RGTextureAccessDef& textureAccessDef : dependencies.textureAccesses)
	{
		const RGTextureView& accessedTextureView = textureAccessDef.textureView;
		const RGTextureHandle accessedTexture = accessedTextureView.GetTexture();
		const rhi::TextureSubresourceRange& accessedSubresourceRange = accessedTextureView.GetViewDefinition().subresourceRange;

		if (accessedTexture->HasAcquiredNode())
		{
			accessedTexture->SetAcquireNode(node);
		}

		RGTextureAccessState& textureAccessState = accessedTexture->GetAccessState();

		const rhi::BarrierTextureTransitionTarget& transitionTarget = GetTransitionDefForAccess(textureAccessDef.access);

		if (textureAccessState.IsFullResource())
		{
			const rhi::BarrierTextureTransitionTarget& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForFullResource().access);
			node->AddTextureState(accessedTexture, accessedSubresourceRange, transitionSource, transitionTarget);
		}
		else
		{
			textureAccessState.ForEachSubresource(accessedSubresourceRange,
												  [&, this](RGTextureSubresource subresource)
												  {
													  const rhi::BarrierTextureTransitionTarget& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForSubresource(subresource));

													  rhi::TextureSubresourceRange subresourceRange;
													  subresourceRange.aspect = accessedSubresourceRange.aspect;
													  subresourceRange.baseArrayLayer = subresource.arrayLayerIdx;
													  subresourceRange.arrayLayersNum = 1;
													  subresourceRange.baseMipLevel = subresource.mipMapIdx;
													  subresourceRange.mipLevelsNum = 1;

													  node->AddTextureState(accessedTexture, subresourceRange, transitionSource, transitionTarget);
												  });
		}

		textureAccessState.SetSubresourcesAccess(RGTextureSubresourceAccessState(textureAccessDef.access, node), accessedSubresourceRange);

		m_nodes.emplace_back(node);
	}
}

} // spt::rg
