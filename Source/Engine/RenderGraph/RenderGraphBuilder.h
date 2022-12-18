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
#include "CommandsRecorder/RenderingDefinition.h"

namespace spt::rhi
{
struct BarrierTextureTransitionDefinition;
} // spt::rhi


namespace spt::rg
{

template<typename... TDescriptorSetStates>
auto BindDescriptorSets(TDescriptorSetStates&&... descriptorSetStates)
{
	constexpr SizeType size = lib::ParameterPackSize<TDescriptorSetStates...>::Count;
	return lib::StaticArray<lib::SharedPtr<rg::RGDescriptorSetStateBase>, size>{ descriptorSetStates... };
}


using RGRenderTargetDef = rhi::RTGenericDefinition<RGTextureViewHandle>;


class RGRenderPassDefinition
{
public:

	RGRenderPassDefinition(math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent, rhi::ERenderingFlags renderingFlags = rhi::ERenderingFlags::Default)
		: m_renderAreaOffset(renderAreaOffset)
		, m_renderAreaExtent(renderAreaExtent)
		, m_renderingFlags(renderingFlags)
	{ }

	void AddColorRenderTarget(const RGRenderTargetDef& definition)
	{
		m_colorRenderTargetDefs.emplace_back(definition);
	}

	void AddDepthRenderTarget(const RGRenderTargetDef& definition)
	{
		m_depthRenderTargetDef = definition;
	}

	void AddStencilRenderTarget(const RGRenderTargetDef& definition)
	{
		m_stencilRenderTargetDef = definition;
	}

	rdr::RenderingDefinition CreateRenderingDefinition() const
	{
		SPT_PROFILER_FUNCTION();

		rdr::RenderingDefinition renderingDefinition(m_renderAreaOffset, m_renderAreaExtent, m_renderingFlags);
	
		std::for_each(std::cbegin(m_colorRenderTargetDefs), std::cend(m_colorRenderTargetDefs),
					  [this, &renderingDefinition](const RGRenderTargetDef& def)
					  {
						  renderingDefinition.AddColorRenderTarget(CreateRTDefinition(def));
					  });

		if (IsRTDefinitionValid(m_depthRenderTargetDef))
		{
			renderingDefinition.AddDepthRenderTarget(CreateRTDefinition(m_depthRenderTargetDef));
		}

		if (IsRTDefinitionValid(m_stencilRenderTargetDef))
		{
			renderingDefinition.AddStencilRenderTarget(CreateRTDefinition(m_stencilRenderTargetDef));
		}

		return renderingDefinition;
	}

private:

	rdr::RTDefinition CreateRTDefinition(const RGRenderTargetDef& rgDef) const
	{
		SPT_CHECK(rgDef.textureView.IsValid());

		rdr::RTDefinition def;

		def.textureView = rgDef.textureView->GetViewInstance();

		if (rgDef.resolveTextureView.IsValid())
		{
			def.resolveTextureView = rgDef.resolveTextureView->GetViewInstance();
		}

		def.loadOperation = rgDef.loadOperation;
		def.storeOperation = rgDef.storeOperation;
		def.resolveMode = rgDef.resolveMode;
		def.clearColor = rgDef.clearColor;

		return def;
	}

	Bool IsRTDefinitionValid(const RGRenderTargetDef& rgDef) const
	{
		return rgDef.textureView.IsValid();
	}

	lib::DynamicArray<RGRenderTargetDef>	m_colorRenderTargetDefs;
	RGRenderTargetDef						m_depthRenderTargetDef;
	RGRenderTargetDef						m_stencilRenderTargetDef;

	math::Vector2i			m_renderAreaOffset;
	math::Vector2u			m_renderAreaExtent;
	rhi::ERenderingFlags	m_renderingFlags;
};


class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder();

	RGTextureHandle AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture);

	RGTextureHandle CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);
	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination, const rhi::TextureSubresourceRange& transitionRange, const rhi::BarrierTextureTransitionDefinition& preExtractionTransitionTarget);

	template<typename TDescriptorSetStatesRange>
	void AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

	template<typename TDescriptorSetStatesRange>
	void AddRenderPass(const RenderGraphDebugName& dispatchName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange);

	void Execute();

private:

	template<typename TNodeType, typename... TArgs>
	TNodeType& AllocateNode(const RenderGraphDebugName& dispatchName, TArgs&&... args);

	template<typename TDescriptorSetStatesRange>
	void BuildDescriptorSetDependencies(TDescriptorSetStatesRange&& dsStatesRange, RGDependeciesContainer& dependencies);

	void AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies);

	const rhi::BarrierTextureTransitionDefinition& GetTransitionDefForAccess(ERGAccess access) const;

	void PostBuild();
	void Compile();
	void ExecuteGraph();

	void AddPrepareTexturesForExtractionNode();

	void ResolveResourceReleases();
	void ResolveTextureReleases();

	lib::DynamicArray<RGTextureHandle> m_textures;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;

	lib::DynamicArray<RGNodeHandle> m_nodes;

	RGAllocator allocator;
};


template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, groupCount](const lib::SharedRef<rdr::RenderContext>& renderContext, const lib::SharedPtr<rdr::CommandRecorder>& recorder)
	{
		recorder->BindComputePipeline(computePipelineID);
		recorder->Dispatch(groupCount);
	};

	using LambdaType = decltype(executeLambda);
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(dispatchName, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	BuildDescriptorSetDependencies(dsStatesRange, dependencies);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::AddRenderPass(const RenderGraphDebugName& dispatchName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange)
{

}

template<typename TNodeType, typename... TArgs>
TNodeType& RenderGraphBuilder::AllocateNode(const RenderGraphDebugName& dispatchName, TArgs&&... args)
{
	SPT_PROFILER_FUNCTION();

	const RGNodeID nodeID = m_nodes.size();

	TNodeType* allocatedNode = allocator.Allocate<TNodeType>(dispatchName, nodeID, std::forward_as_tuple<TArgs>(args)...);
	SPT_CHECK(!!allocatedNode);

	return *allocatedNode;
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
