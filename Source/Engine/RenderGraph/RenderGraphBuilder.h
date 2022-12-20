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
	return lib::StaticArray<lib::SharedRef<rg::RGDescriptorSetStateBase>, size>{ descriptorSetStates... };
}


decltype(auto) EmptyDescriptorSets()
{
	static lib::StaticArray<lib::SharedRef<rg::RGDescriptorSetStateBase>, 0> empty;
	return empty;
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
	
	SPT_NODISCARD RGRenderTargetDef& AddColorRenderTarget()
	{
		return m_colorRenderTargetDefs.emplace_back();
	}

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

	void BuildDependencies(RGDependenciesBuilder& dependenciesBuilder) const
	{
		SPT_PROFILER_FUNCTION();

		for (const RGRenderTargetDef& colorRenderTarget : m_colorRenderTargetDefs)
		{
			SPT_CHECK(colorRenderTarget.textureView.IsValid());

			dependenciesBuilder.AddTextureAccess(colorRenderTarget.textureView, ERGAccess::ColorRenderTarget);
		}

		if (m_depthRenderTargetDef.textureView.IsValid())
		{
			dependenciesBuilder.AddTextureAccess(m_depthRenderTargetDef.textureView, ERGAccess::DepthRenderTarget);
		}
		
		if (m_stencilRenderTargetDef.textureView.IsValid())
		{
			dependenciesBuilder.AddTextureAccess(m_stencilRenderTargetDef.textureView, ERGAccess::StencilRenderTarget);
		}
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

	RGTextureViewHandle CreateTextureView(const RenderGraphDebugName& name, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition, ERGResourceFlags flags = ERGResourceFlags::Default);

	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);
	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination, const rhi::TextureSubresourceRange& transitionRange, const rhi::BarrierTextureTransitionDefinition& preExtractionTransitionTarget);

	template<typename TDescriptorSetStatesRange>
	void AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

	template<typename TDescriptorSetStatesRange, typename TCallable>
	void AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	void BindDescriptorSetState(const lib::SharedPtr<rdr::DescriptorSetState>& dsState);
	void UnbindDescriptorSetState(const lib::SharedPtr<rdr::DescriptorSetState>& dsState);

	void Execute(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores);

private:

	template<typename TNodeType, typename... TArgs>
	TNodeType& AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args);

	template<typename TDescriptorSetStatesRange>
	void BuildDescriptorSetDependencies(TDescriptorSetStatesRange&& dsStatesRange, RGDependeciesContainer& dependencies);

	void AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies);

	const rhi::BarrierTextureTransitionDefinition& GetTransitionDefForAccess(RGNodeHandle node, ERGAccess access) const;

	Bool RequiresSynchronization(const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget) const;

	void PostBuild();
	void ExecuteGraph(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores);

	void AddPrepareTexturesForExtractionNode();

	void ResolveResourceReleases();
	void ResolveTextureReleases();

	lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> GetExternalDSStates() const;

	lib::DynamicArray<RGTextureHandle> m_textures;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;

	lib::DynamicArray<RGNodeHandle> m_nodes;

	lib::DynamicArray<lib::SharedPtr<rdr::DescriptorSetState>> m_boundDSStates;

	RGAllocator m_allocator;
};

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, groupCount, dsStatesRange, externalBoundStates = GetExternalDSStates()](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindDescriptorSetStates(externalBoundStates);
		recorder.BindDescriptorSetStates(dsStatesRange);

		recorder.BindComputePipeline(computePipelineID);
		recorder.Dispatch(groupCount);

		recorder.UnbindDescriptorSetStates(dsStatesRange);
		recorder.UnbindDescriptorSetStates(externalBoundStates);
	};

	using LambdaType = std::remove_cv_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(dispatchName, ERenderGraphNodeType::Dispatch, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	BuildDescriptorSetDependencies(dsStatesRange, dependencies);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TCallable>
void RenderGraphBuilder::AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [renderPassDef, dsStatesRange, callable, externalBoundStates = GetExternalDSStates()](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const rdr::RenderingDefinition renderingDefinition = renderPassDef.CreateRenderingDefinition();

		recorder.BeginRendering(renderingDefinition);

		recorder.BindDescriptorSetStates(externalBoundStates);
		recorder.BindDescriptorSetStates(dsStatesRange);

		callable(renderContext, recorder);

		recorder.UnbindDescriptorSetStates(dsStatesRange);
		recorder.UnbindDescriptorSetStates(externalBoundStates);

		recorder.EndRendering();
	};

	using LambdaType = std::remove_cv_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(renderPassName, ERenderGraphNodeType::RenderPass, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	BuildDescriptorSetDependencies(dsStatesRange, dependencies);

	RGDependenciesBuilder renderTargetsDependenciesBuilder(dependencies);
	renderPassDef.BuildDependencies(renderTargetsDependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TNodeType, typename... TArgs>
TNodeType& RenderGraphBuilder::AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args)
{
	SPT_PROFILER_FUNCTION();

	const RGNodeID nodeID = m_nodes.size();
	
	TNodeType* allocatedNode = m_allocator.Allocate<TNodeType>(name, nodeID, type, std::forward<TArgs>(args)...);
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
