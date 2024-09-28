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
#include "RGNodeParametersStruct.h"
#include "Utility/Templates/Overload.h"
#include "JobSystem.h"

#define SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME 0

namespace spt::rhi
{
struct BarrierTextureTransitionDefinition;
} // spt::rhi


namespace spt::rdr
{
class Pipeline;
} // spt::rdr


namespace spt::rg
{

class RenderGraphDebugDecorator;
class RenderGraphResourcesPool;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Descriptor Sets Helpers =======================================================================

template<typename... TDescriptorSetStates>
auto BindDescriptorSets(TDescriptorSetStates&&... descriptorSetStates)
{
	constexpr SizeType size = lib::ParameterPackSize<TDescriptorSetStates...>::Count;
	return lib::StaticArray<lib::MTHandle<rg::RGDescriptorSetStateBase>, size>{ lib::MTHandle<rg::RGDescriptorSetStateBase>(std::forward<TDescriptorSetStates>(descriptorSetStates))... };
}


inline decltype(auto) EmptyDescriptorSets()
{
	static lib::StaticArray<lib::MTHandle<rg::RGDescriptorSetStateBase>, 0> empty;
	return empty;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Render Graph ==================================================================================

class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder(RenderGraphResourcesPool& resourcesPool);
	~RenderGraphBuilder();

	// Utility ================================================

	void BindGPUStatisticsCollector(const lib::SharedRef<rdr::GPUStatisticsCollector>& collector);

	void AddDebugDecorator(const lib::SharedRef<RenderGraphDebugDecorator>& debugDecorator);

	// Textures ===============================================

	Bool IsTextureAcquired(const lib::SharedPtr<rdr::Texture>& texture) const;

	RGTextureHandle AcquireExternalTexture(const lib::SharedPtr<rdr::Texture>& texture);
	
	RGTextureViewHandle TryAcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView);
	RGTextureViewHandle AcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView);

	RGTextureHandle CreateTexture(const RenderGraphDebugName& name, const TextureDef& textureDefinition, const std::optional<rhi::RHIAllocationInfo>& allocationInfo = std::nullopt, ERGResourceFlags flags = ERGResourceFlags::Default);

	RGTextureViewHandle CreateTextureView(const RenderGraphDebugName& name, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition = rhi::TextureViewDefinition(), ERGResourceFlags flags = ERGResourceFlags::Default);
	
	/** Creates texture from given definition and returns full view of this texture */
	RGTextureViewHandle CreateTextureView(const RenderGraphDebugName& name, const TextureDef& textureDefinition, const std::optional<rhi::RHIAllocationInfo>& allocationInfo = std::nullopt, ERGResourceFlags flags = ERGResourceFlags::Default);
	
	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);

	void ReleaseTextureWithTransition(RGTextureHandle textureHandle, const rhi::BarrierTextureTransitionDefinition& releaseTransitionTarget);
	
	// Buffers ================================================

	RGBufferHandle AcquireExternalBuffer(const lib::SharedPtr<rdr::Buffer>& buffer);

	RGBufferViewHandle AcquireExternalBufferView(const rdr::BufferView& bufferView);

	RGBufferHandle CreateBuffer(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	RGBufferViewHandle CreateBufferView(const RenderGraphDebugName& name, RGBufferHandle buffer, Uint64 offset, Uint64 size, ERGResourceFlags flags = ERGResourceFlags::Default);

	/** Creates buffer and returns view of full buffer */
	RGBufferViewHandle CreateBufferView(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	void ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination);
	
	// Utilities ==============================================

	RenderGraphResourcesPool& GetResourcesPool() const;

	template<typename TType, typename... TArgs>
	TType* Allocate(TArgs&&... args);

	template<typename TDSType>
	lib::MTHandle<TDSType> CreateDescriptorSet(const rdr::RendererResourceName& name);

	// Diagnostics ============================================

#if RG_ENABLE_DIAGNOSTICS
	void PushProfilerScope(lib::HashedString name);
	void PopProfilerScope();
#endif // RG_ENABLE_DIAGNOSTICS
	
	// Commands ===============================================

	/** Calls dispatch command with given descriptor sets (this version automatically creates pipeline from shader */
	template<typename TDescriptorSetStatesRange>
	void Dispatch(const RenderGraphDebugName& dispatchName, rdr::ShaderID shader, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

	/** Calls dispatch command with given descriptor sets */
	template<typename TDescriptorSetStatesRange>
	void Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

	/** Calls dispatch indirect command with given descriptor sets */
	template<typename TDescriptorSetStatesRange>
	void DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange);

	/** Creates render pass with given descriptor sets and executes callable inside it */
	template<typename TDescriptorSetStatesRange, typename TCallable>
	void RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	/** Creates render pass with given descriptor sets and executes callable inside it */
	template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
	void RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable);

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TDescriptorSetStatesRange, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable);

	template<typename TDescriptorSetStatesRange>
	void TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, TDescriptorSetStatesRange&& dsStatesRange);

	template<typename TDescriptorSetStatesRange>
	void TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange);

	void FillBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range, Uint32 data);

	void FillFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint32 data);

	void CopyFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, RGBufferViewHandle destBufferView);

	void CopyBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, Uint64 sourceOffset, RGBufferViewHandle destBufferView, Uint64 destOffset, Uint64 range);

	lib::SharedRef<rdr::Buffer> DownloadBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range);

	void CopyTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, const math::Vector3i& sourceOffset, RGTextureViewHandle destRGTextureView, const math::Vector3i& destOffset, const math::Vector3u& copyExtent);
	
	void CopyFullTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, RGTextureViewHandle destRGTextureView);

	void ClearTexture(const RenderGraphDebugName& clearName, RGTextureViewHandle textureView, const rhi::ClearColor& clearColor);

	void BindDescriptorSetState(const lib::MTHandle<RGDescriptorSetStateBase>& dsState);
	void UnbindDescriptorSetState(const lib::MTHandle<RGDescriptorSetStateBase>& dsState);

	const js::Event& GetGPUFinishedEvent() const;

	void Execute();

private:

	template<typename TNodeType, typename... TArgs>
	TNodeType& AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args);

	template<typename TDescriptorSetStatesRange, typename TCallable>
	RGNode& CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	template<typename TParametersTuple>
	void BuildParametersDependencies(const TParametersTuple& parametersStructs, RGDependenciesBuilder& dependenciesBuilder) const;
	
	template<typename TParameters>
	void BuildParametersStructDependencies(const TParameters& parameters, RGDependenciesBuilder& dependenciesBuilder) const;

	void AssignDescriptorSetsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<lib::MTHandle<RGDescriptorSetStateBase> const> dsStatesRange, RGDependenciesBuilder& dependenciesBuilder);

	void AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies);
	void PostNodeAdded(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies);

	const rhi::BarrierTextureTransitionDefinition& GetTransitionDefForAccess(RGNodeHandle node, ERGTextureAccess access) const;

	void GetSynchronizationParamsForBuffer(ERGBufferAccess lastAccess, rhi::EAccessType& outAccessType) const;

	Bool RequiresSynchronization(const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget) const;
	Bool RequiresSynchronization(RGBufferHandle buffer, rhi::EPipelineStage prevAccessStage, ERGBufferAccess prevAccess, ERGBufferAccess nextAccess, rhi::EPipelineStage nextAccessStage) const;

	void PostBuild();
	void ExecuteGraph();

	void AddReleaseResourcesNode();

	void ResolveResourceProperties();
	void ResolveTextureProperties();
	void ResolveBufferReleases();

	rdr::PipelineStateID GetOrCreateComputePipelineStateID(rdr::ShaderID shader) const;

	lib::SharedPtr<rdr::Pipeline> GetPipelineObject(rdr::PipelineStateID psoID) const;

	lib::DynamicArray<RGTextureHandle> m_textures;
	lib::DynamicArray<RGBufferHandle> m_buffers;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;
	lib::HashMap<lib::SharedPtr<rdr::Buffer>, RGBufferHandle> m_externalBuffers;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;
	lib::DynamicArray<RGBufferHandle> m_extractedBuffers;

	lib::DynamicArray<RGNodeHandle> m_nodes;

	lib::DynamicArray<lib::MTHandle<RGDescriptorSetStateBase>> m_boundDSStates;

	lib::DynamicArray<lib::SharedPtr<RenderGraphDebugDecorator>> m_debugDecorators;

	lib::SharedPtr<rdr::GPUStatisticsCollector> m_statisticsCollector;

	RGResourceHandle<RGRenderPassNodeBase> m_lastRenderPassNode;

#if SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	lib::DynamicArray<lib::MTHandle<RGDescriptorSetStateBase>> m_allocatedDSStates;
#endif // SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME

	js::Event m_onGraphExecutionFinished;

	RenderGraphResourcesPool& m_resourcesPool;

#if RG_ENABLE_DIAGNOSTICS
	RGProfilerRecorder m_profilerRecorder;
#endif // RG_ENABLE_DIAGNOSTICS

	lib::SharedPtr<rdr::DescriptorSetStackAllocator> m_dsAllocator;

	RGAllocator m_allocator;
};

template<typename TType, typename... TArgs>
TType* RenderGraphBuilder::Allocate(TArgs&&... args)
{
	return m_allocator.Allocate<TType>(std::forward<TArgs>(args)...);
}

template<typename TDSType>
lib::MTHandle<TDSType> RenderGraphBuilder::CreateDescriptorSet(const rdr::RendererResourceName& name)
{
	rdr::DescriptorSetStateParams params;
	params.stackAllocator = m_dsAllocator;
	lib::MTHandle<TDSType> ds = m_allocator.AllocateUntracked<TDSType>(name, params);
	ds->DisableDeleteOnZeroRefCount();
#if SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	m_allocatedDSStates.emplace_back(ds);
#endif // SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	return ds;
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::Dispatch(const RenderGraphDebugName& dispatchName, rdr::ShaderID shader, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	const rdr::PipelineStateID pipelineStateID = GetOrCreateComputePipelineStateID(shader);
	Dispatch(dispatchName, pipelineStateID, groupCount, std::forward<TDescriptorSetStatesRange>(dsStatesRange));
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, groupCount, dsStatesRange](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindComputePipeline(computePipelineID);
		recorder.Dispatch(groupCount.AsVector());
		recorder.UnbindComputePipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(dispatchName, ERenderGraphNodeType::Dispatch, std::move(executeLambda));

#if DEBUG_RENDER_GRAPH
	RGNodeComputeDebugMetaData debugMetaData;
	debugMetaData.pipelineStateID = computePipelineID;
	node.SetDebugMetaData(debugMetaData);
#endif // DEBUG_RENDER_GRAPH

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ComputeShader);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(computePipelineID), { dsStatesRange }, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, indirectArgsBuffer, indirectArgsOffset, dsStatesRange](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindComputePipeline(computePipelineID);
		recorder.DispatchIndirect(indirectArgsBuffer->GetResource(), indirectArgsOffset);
		recorder.UnbindComputePipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(dispatchName, ERenderGraphNodeType::Dispatch, std::move(executeLambda));

#if DEBUG_RENDER_GRAPH
	RGNodeComputeDebugMetaData debugMetaData;
	debugMetaData.pipelineStateID = computePipelineID;
	node.SetDebugMetaData(debugMetaData);
#endif // DEBUG_RENDER_GRAPH

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ComputeShader);

	AssignDescriptorSetsToNode(node, GetPipelineObject(computePipelineID), { dsStatesRange }, dependenciesBuilder);

	dependenciesBuilder.AddBufferAccess(indirectArgsBuffer, ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TCallable>
void RenderGraphBuilder::RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	RenderPass(renderPassName, renderPassDef, dsStatesRange, std::make_tuple(), callable);
}

template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();
	
	RGNode& node = CreateRenderPassNodeInternal(renderPassName, renderPassDef, dsStatesRange, std::forward<TCallable>(callable));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);
	
	AssignDescriptorSetsToNode(node, nullptr, { dsStatesRange }, dependenciesBuilder);
	
	renderPassDef.BuildDependencies(dependenciesBuilder);
	BuildParametersDependencies(parameters, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	AddSubpass(subpassName, dsStatesRange, std::make_tuple(), std::forward<TCallable>(callable));
}

template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_lastRenderPassNode);

	using CallableType	= std::remove_cvref_t<TCallable>;
	using SubpassType	= RGLambdaSubpass<CallableType>;
	RGSubpassHandle subpass = m_allocator.Allocate<SubpassType>(subpassName, std::forward<TCallable>(callable));

	for (const lib::MTHandle<rdr::DescriptorSetState>& dsState : dsStatesRange)
	{
		if (dsState.IsValid())
		{
			subpass->BindDSState(dsState);
		}
	}

	for (const lib::MTHandle<rdr::DescriptorSetState>& dsState : m_boundDSStates)
	{
		subpass->BindDSState(dsState);
	}
	
	m_lastRenderPassNode->AppendSubpass(subpass);

	RGDependeciesContainer subpassDependencies;
	RGDependenciesBuilder subpassDependenciesBuilder(*this, subpassDependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);
	
	BuildParametersDependencies(parameters, subpassDependenciesBuilder);
	AssignDescriptorSetsToNode(*m_lastRenderPassNode, nullptr, { dsStatesRange }, subpassDependenciesBuilder);

	ResolveNodeDependecies(*m_lastRenderPassNode, subpassDependencies);
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [ rayTracingPipelineID, traceCount, dsStatesRange ](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindRayTracingPipeline(rayTracingPipelineID);
		recorder.TraceRays(traceCount.AsVector());
		recorder.UnbindRayTracingPipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(traceName, ERenderGraphNodeType::TraceRays, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::RayTracingShader);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(rayTracingPipelineID), { dsStatesRange }, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [ rayTracingPipelineID, indirectArgsBuffer, indirectArgsOffset, dsStatesRange ](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindRayTracingPipeline(rayTracingPipelineID);
		recorder.TraceRaysIndirect(indirectArgsBuffer->GetResource(), indirectArgsOffset);
		recorder.UnbindRayTracingPipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(traceName, ERenderGraphNodeType::TraceRays, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::RayTracingShader);

	dependenciesBuilder.AddBufferAccess(indirectArgsBuffer, ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(rayTracingPipelineID), { dsStatesRange }, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}


template<typename TNodeType, typename... TArgs>
TNodeType& RenderGraphBuilder::AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args)
{
	SPT_PROFILER_FUNCTION();

	const RGNodeID nodeID = m_nodes.size();
	
	TNodeType* allocatedNode = m_allocator.Allocate<TNodeType>(*this, name, nodeID, type, std::forward<TArgs>(args)...);
	SPT_CHECK(!!allocatedNode);

	return *allocatedNode;
}

template<typename TDescriptorSetStatesRange, typename TCallable>
RGNode& RenderGraphBuilder::CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	using LambdaType = std::remove_cvref_t<TCallable>;
	using NodeType = RGRenderPassNode<LambdaType>;

	return AllocateNode<NodeType>(renderPassName, ERenderGraphNodeType::RenderPass, renderPassDef, callable);
}

template<typename TParametersTuple>
void RenderGraphBuilder::BuildParametersDependencies(const TParametersTuple& parametersTuple, RGDependenciesBuilder& dependenciesBuilder) const
{
	std::apply([this, &dependenciesBuilder](const auto&... parameters)
			   {
				   (BuildParametersStructDependencies(parameters, dependenciesBuilder), ...);
			   },
			   parametersTuple);
}

template<typename TParameters>
void RenderGraphBuilder::BuildParametersStructDependencies(const TParameters& parameters, RGDependenciesBuilder& dependenciesBuilder) const
{
	ForEachRGParameterAccess(parameters,
							 lib::Overload([&dependenciesBuilder](RGBufferViewHandle buffer, ERGBufferAccess access, rhi::EPipelineStage pipelineStages)
										   {
											   if (buffer.IsValid())
											   {
												   dependenciesBuilder.AddBufferAccess(buffer, access, pipelineStages);
											   }
										   },
										   [&dependenciesBuilder](RGTextureViewHandle texture, ERGTextureAccess access, rhi::EPipelineStage pipelineStages)
										   {
											   if (texture.IsValid())
											   {
												   dependenciesBuilder.AddTextureAccess(texture, access, pipelineStages);
											   }
										   }));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Render Graph Utilities ========================================================================

template<typename TDSRange>
class BindDescriptorSetsScope
{
public:

	BindDescriptorSetsScope(RenderGraphBuilder& graphBuilder, const TDSRange& descriptorSets)
		: m_graphBuilder(graphBuilder)
		, m_descriptorSets(descriptorSets)
	{
		for (const lib::MTHandle<RGDescriptorSetStateBase>& ds : m_descriptorSets)
		{
			if (ds.IsValid())
			{
				m_graphBuilder.BindDescriptorSetState(ds);
			}
		}
	}

	~BindDescriptorSetsScope()
	{
		for (const lib::MTHandle<RGDescriptorSetStateBase>& ds : m_descriptorSets)
		{
			if (ds.IsValid())
			{
				m_graphBuilder.UnbindDescriptorSetState(ds);
			}
		}
	}

private:

	RenderGraphBuilder& m_graphBuilder;
	TDSRange m_descriptorSets;
};

} // spt::rg
