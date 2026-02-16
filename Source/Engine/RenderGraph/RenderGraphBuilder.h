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
// Shader Params =================================================================================

struct EmptyShaderParams {};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Render Graph ==================================================================================

class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder(lib::MemoryArena& memoryArena, RenderGraphResourcesPool& resourcesPool);
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

	RGTextureViewHandle CreateTextureMipView(RGTextureHandle texture, Uint32 mipLevel, Uint32 arrayLayer = 0u);
	RGTextureViewHandle CreateTextureMipView(RGTextureViewHandle texture, Uint32 mipLevel, Uint32 arrayLayer = 0u);
	
	void ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination);

	void ReleaseTextureWithTransition(RGTextureHandle textureHandle, const rhi::BarrierTextureTransitionDefinition& releaseTransitionTarget);
	
	// Buffers ================================================

	RGBufferHandle AcquireExternalBuffer(const lib::SharedPtr<rdr::Buffer>& buffer);

	RGBufferViewHandle AcquireExternalBufferView(lib::SharedPtr<rdr::BindableBufferView> bufferView);

	RGBufferHandle CreateBuffer(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	RGBufferViewHandle CreateBufferView(const RenderGraphDebugName& name, RGBufferHandle buffer, Uint64 offset, Uint64 size, ERGResourceFlags flags = ERGResourceFlags::Default);

	/** Creates buffer and returns view of full buffer */
	RGBufferViewHandle CreateBufferView(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	RGBufferViewHandle CreateStorageBufferView(const RenderGraphDebugName& name, Uint32 size, const rhi::RHIAllocationInfo& allocationInfo = rhi::EMemoryUsage::GPUOnly);

	void ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination);
	
	// Utilities ==============================================

	RenderGraphResourcesPool& GetResourcesPool() const;

	template<typename TType, typename... TArgs>
	TType* Allocate(TArgs&&... args);

	RGAllocator& GetAllocator() { return m_allocator; }

	lib::MemoryArena& GetMemoryArena() { return m_memoryArena; }

	template<typename TDSType>
	lib::MTHandle<TDSType> CreateDescriptorSet(const rdr::RendererResourceName& name);

	// Diagnostics ============================================

#if RG_ENABLE_DIAGNOSTICS
	void PushProfilerScope(lib::HashedString name);
	void PopProfilerScope();
#endif // RG_ENABLE_DIAGNOSTICS
	
	// Commands ===============================================

	/** Calls dispatch command with given descriptor sets (this version automatically creates pipeline from shader */
	template<typename TDescriptorSetStatesRange, typename TShaderParams = EmptyShaderParams>
	void Dispatch(const RenderGraphDebugName& dispatchName, rdr::ShaderID shader, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams = TShaderParams{});

	/** Calls dispatch command with given descriptor sets */
	template<typename TDescriptorSetStatesRange, typename TShaderParams = EmptyShaderParams>
	void Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams = TShaderParams{});

	/** Calls dispatch indirect command with given descriptor sets */
	template<typename TDescriptorSetStatesRange, typename TShaderParams = EmptyShaderParams>
	void DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams = TShaderParams{});

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

	template<typename TDescriptorSetStatesRange, typename TShaderParams = EmptyShaderParams>
	void TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams = TShaderParams{});

	template<typename TDescriptorSetStatesRange, typename TShaderParams = EmptyShaderParams>
	void TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams = TShaderParams{});

	template<typename TPassParameters, typename TCallable>
	void AddLambdaPass(const RenderGraphDebugName& passName, const TPassParameters& parameters, TCallable&& callable);

	void FillBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range, Uint32 data);

	void FillFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint32 data);

	void MemZeroBuffer(RGBufferViewHandle bufferView);

	void CopyFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, RGBufferViewHandle destBufferView);

	void CopyBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, Uint64 sourceOffset, RGBufferViewHandle destBufferView, Uint64 destOffset, Uint64 range);

	lib::SharedRef<rdr::Buffer>  DownloadBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range);
	lib::SharedRef<rdr::Buffer>  DownloadTextureToBuffer(const RenderGraphDebugName& commandName, RGTextureViewHandle textureView);
	lib::SharedRef<rdr::Texture> DownloadTexture(const RenderGraphDebugName& commandName, RGTextureViewHandle textureView);

	void CopyTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, const math::Vector3i& sourceOffset, RGTextureViewHandle destRGTextureView, const math::Vector3i& destOffset, const math::Vector3u& copyExtent);
	
	void CopyFullTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, RGTextureViewHandle destRGTextureView);

	void BlitTexture(const RenderGraphDebugName& blitName, rg::RGTextureViewHandle source, rg::RGTextureViewHandle dest, rhi::ESamplerFilterType filterMode);

	void CopyTextureToBuffer(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, RGBufferViewHandle destBufferView, Uint64 bufferOffset);
	void CopyBufferToFullTexture(const RenderGraphDebugName& copyName, RGBufferViewHandle sourceBufferView, Uint64 bufferOffset, RGTextureViewHandle destRGTextureView);

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

	template<typename TShaderParams>
	void AssignShaderParamsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParams& shaderParams, RGDependenciesBuilder& dependenciesBuilder);

	void AssignDescriptorSetsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<lib::MTHandle<RGDescriptorSetStateBase> const> dsStatesRange, RGDependenciesBuilder& dependenciesBuilder);

	Bool AssignShaderParamsToNodeInternal(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<const Byte> paramsData, const lib::HashedString& paramsType, RGDependenciesBuilder& dependenciesBuilder);

	void AddNodeInternal(RGNode& node, RGDependeciesContainer& dependencies);
	void PostNodeAdded(RGNode& node, const RGDependeciesContainer& dependencies);
	void PostSubpassAdded(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void RevertGloballyReadableState(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange);

	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies);

	const rhi::BarrierTextureTransitionDefinition& GetTransitionDefForAccess(RGNodeHandle node, rg::RGTextureHandle texture, ERGTextureAccess access) const;

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

	rdr::DescriptorSetStateParams BuildDesctiptorSetStateParams();

	lib::DynamicPushArray<RGTexture>     m_textures;
	lib::DynamicPushArray<RGTextureView> m_textureViews;
	lib::DynamicPushArray<RGBuffer>      m_buffers;
	lib::DynamicPushArray<RGBufferView>  m_bufferViews;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;
	lib::HashMap<lib::SharedPtr<rdr::Buffer>, RGBufferHandle> m_externalBuffers;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;
	lib::DynamicArray<RGBufferHandle> m_extractedBuffers;

	lib::DynamicPushArray<RGNodeHandle> m_nodes;
	RGNodeID m_nodeCounter = 0u;

	lib::DynamicArray<lib::MTHandle<RGDescriptorSetStateBase>> m_boundDSStates;

	lib::DynamicArray<lib::SharedPtr<RenderGraphDebugDecorator>> m_debugDecorators;

	lib::SharedPtr<rdr::GPUStatisticsCollector> m_statisticsCollector;

	RGResourceHandle<RGRenderPassNodeBase> m_lastRenderPassNode;

	lib::DynamicArray<RGTextureViewHandle> m_pendingGloballyReadableTransitions;

#if SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	lib::DynamicArray<lib::MTHandle<RGDescriptorSetStateBase>> m_allocatedDSStates;
#endif // SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME

	js::Event m_onGraphExecutionFinished;

	RenderGraphResourcesPool& m_resourcesPool;

#if RG_ENABLE_DIAGNOSTICS
	RGProfilerRecorder m_profilerRecorder;
#endif // RG_ENABLE_DIAGNOSTICS

	rdr::DescriptorStackAllocator m_dsAllocator;

	RGAllocator m_allocator;

	lib::MemoryArena& m_memoryArena;
};

template<typename TType, typename... TArgs>
TType* RenderGraphBuilder::Allocate(TArgs&&... args)
{
	return m_allocator.Allocate<TType>(std::forward<TArgs>(args)...);
}

template<typename TDSType>
lib::MTHandle<TDSType> RenderGraphBuilder::CreateDescriptorSet(const rdr::RendererResourceName& name)
{
	lib::MTHandle<TDSType> ds = m_allocator.AllocateUntracked<TDSType>(name, BuildDesctiptorSetStateParams());
	ds->DisableDeleteOnZeroRefCount();
#if SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	m_allocatedDSStates.emplace_back(ds);
#endif // SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	return ds;
}

template<typename TDescriptorSetStatesRange, typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::Dispatch(const RenderGraphDebugName& dispatchName, rdr::ShaderID shader, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const rdr::PipelineStateID pipelineStateID = GetOrCreateComputePipelineStateID(shader);
	Dispatch(dispatchName, pipelineStateID, groupCount, std::forward<TDescriptorSetStatesRange>(dsStatesRange), shaderParams);
}

template<typename TDescriptorSetStatesRange, typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
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

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ComputeShader);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(computePipelineID), { dsStatesRange }, dependenciesBuilder);

	AssignShaderParamsToNode(node, GetPipelineObject(computePipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const auto executeLambda = [computePipelineID, indirectArgsBuffer, indirectArgsOffset, dsStatesRange](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindComputePipeline(computePipelineID);
		recorder.DispatchIndirect(*indirectArgsBuffer->GetResource(), indirectArgsOffset);
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

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ComputeShader);

	AssignDescriptorSetsToNode(node, GetPipelineObject(computePipelineID), { dsStatesRange }, dependenciesBuilder);

	dependenciesBuilder.AddBufferAccess(indirectArgsBuffer, ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect);

	AssignShaderParamsToNode(node, GetPipelineObject(computePipelineID), shaderParams, dependenciesBuilder);

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
	RGNode& node = CreateRenderPassNodeInternal(renderPassName, renderPassDef, dsStatesRange, std::forward<TCallable>(callable));

	RGDependeciesContainer dependencies(m_memoryArena);
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
	SPT_CHECK(!!m_lastRenderPassNode);

	using CallableType = std::remove_cvref_t<TCallable>;
	using SubpassType  = RGLambdaSubpass<CallableType>;
	RGSubpassHandle subpass = m_memoryArena.AllocateType<SubpassType>(m_memoryArena, subpassName, std::forward<TCallable>(callable));

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

	RGDependeciesContainer subpassDependencies(m_memoryArena);
	RGDependenciesBuilder subpassDependenciesBuilder(*this, subpassDependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);
	
	BuildParametersDependencies(parameters, subpassDependenciesBuilder);
	AssignDescriptorSetsToNode(*m_lastRenderPassNode, nullptr, { dsStatesRange }, subpassDependenciesBuilder);

	ResolveNodeDependecies(*m_lastRenderPassNode, subpassDependencies);

	PostSubpassAdded(*m_lastRenderPassNode, subpassDependencies);
}

	template<typename TDescriptorSetStatesRange, typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const auto executeLambda = [ rayTracingPipelineID, traceCount ](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindRayTracingPipeline(rayTracingPipelineID);
		recorder.TraceRays(traceCount.AsVector());
		recorder.UnbindRayTracingPipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(traceName, ERenderGraphNodeType::TraceRays, std::move(executeLambda));

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::RayTracingShader);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(rayTracingPipelineID), { dsStatesRange }, dependenciesBuilder);

	AssignShaderParamsToNode(node, GetPipelineObject(rayTracingPipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

	template<typename TDescriptorSetStatesRange, typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, TDescriptorSetStatesRange&& dsStatesRange, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const auto executeLambda = [ rayTracingPipelineID, indirectArgsBuffer, indirectArgsOffset ](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindRayTracingPipeline(rayTracingPipelineID);
		recorder.TraceRaysIndirect(*indirectArgsBuffer->GetResource(), indirectArgsOffset);
		recorder.UnbindRayTracingPipeline();
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(traceName, ERenderGraphNodeType::TraceRays, std::move(executeLambda));

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::RayTracingShader);

	dependenciesBuilder.AddBufferAccess(indirectArgsBuffer, ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect);
	
	AssignDescriptorSetsToNode(node, GetPipelineObject(rayTracingPipelineID), { dsStatesRange }, dependenciesBuilder);

	AssignShaderParamsToNode(node, GetPipelineObject(rayTracingPipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TPassParameters, typename TCallable>
void RenderGraphBuilder::AddLambdaPass(const RenderGraphDebugName& passName, const TPassParameters& parameters, TCallable&& callable)
{
	using LambdaType = std::remove_cvref_t<decltype(callable) > ;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(passName, ERenderGraphNodeType::Generic, std::move(callable));

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::None);

	BuildParametersDependencies(parameters, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TNodeType, typename... TArgs>
TNodeType& RenderGraphBuilder::AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args)
{
	const RGNodeID nodeID = m_nodeCounter++;

	TNodeType* allocatedNode = m_memoryArena.AllocateType<TNodeType>(*this, name, nodeID, type, std::forward<TArgs>(args)...);
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

template<typename TShaderParams>
void RenderGraphBuilder::AssignShaderParamsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParams& shaderParams, RGDependenciesBuilder& dependenciesBuilder)
{
	if constexpr (!std::is_same_v<TShaderParams, EmptyShaderParams>)
	{
		const rdr::HLSLStorage<TShaderParams> shaderParamsHLSLData = shaderParams;

		const Bool assignedParams = AssignShaderParamsToNodeInternal(node, pipeline, shaderParamsHLSLData.GetHLSLDataSpan(), TShaderParams::GetStructName(), dependenciesBuilder);

		if (assignedParams)
		{
			rg::CollectStructDependencies<TShaderParams>(shaderParamsHLSLData.GetHLSLDataSpan(), dependenciesBuilder);
		}
	}
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
