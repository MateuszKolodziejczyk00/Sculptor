#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "Pipelines/PipelineState.h"
#include "RGResources/RGResources.h"
#include "RGResources/RGAllocator.h"
#include "DependenciesBuilder.h"
#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RGNodeParametersStruct.h"
#include "Utility/Templates/Callable.h"
#include "Utility/Templates/Overload.h"
#include "Utils/ConstantsAllocator.h"
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
class RenderGraphBuilder;

//////////////////////////////////////////////////////////////////////////////////////////////////
// Shader Params =================================================================================

struct EmptyShaderParams {};

template<typename... TParams>
inline decltype(auto) ShaderParams(TParams&&... params)
{
	return std::forward_as_tuple(std::forward<TParams>(params)...);
}


struct GenericShaderParamsPtr
{
public:

	GenericShaderParamsPtr() = default;

	template<typename TShaderParams>
	GenericShaderParamsPtr(const rdr::GPUPtr<TShaderParams>& shaderParams);

	GenericShaderParamsPtr(const GenericShaderParamsPtr& rhs) = default;
	GenericShaderParamsPtr& operator=(const GenericShaderParamsPtr& rhs) = default;

	void AssignToNode(RenderGraphBuilder& graphBuilder, RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, RGDependenciesBuilder& dependenciesBuilder) const;
	void AssignToSubpass(RenderGraphBuilder& graphBuilder, RGSubpass& subpass, RGDependenciesBuilder& dependenciesBuilder) const;

	Bool IsValid() const { return m_data.IsValid(); }

private:

	using NodeBinderType    = lib::RawCallable<void(RenderGraphBuilder&, RGNode&, const lib::SharedPtr<rdr::Pipeline>&, const rdr::GPUPtr<void>&, RGDependenciesBuilder&)>;
	using SubpassBinderType = lib::RawCallable<void(RenderGraphBuilder&, RGSubpass&, const rdr::GPUPtr<void>&, RGDependenciesBuilder&)>;

	rdr::GPUPtr<void> m_data;
	NodeBinderType    m_nodeBinder;
	SubpassBinderType m_subpassBinder;
};

//////////////////////////////////////////////////////////////////////////////////////////////////
// Acceleration Structures =======================================================================

struct BLASBuildCommand
{
	lib::SharedPtr<rdr::BottomLevelAS> blas;
	RGBufferViewHandle                 vertexBufferView;
	Uint32                             vertexBufferOffset = 0u;
	RGBufferViewHandle                 indexBufferView;
	Uint32                             indexBufferOffset = 0u;
	Uint32                             primitivesNum = 0u;
	Uint32                             vertexLocationsStride = sizeof(math::Vector3f);

	RGBufferViewHandle scratchBufferView;
	Uint64             scratchBufferOffset = 0u;
};


struct TLASBuildCommand
{
	lib::SharedPtr<rdr::TopLevelAS> tlas;
	RGBufferViewHandle              instanceDefsBufferView;
	Uint32                          instancesNum = 0u;

	RGBufferViewHandle scratchBufferView;
	Uint64             scratchBufferOffset = 0u;
};

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

	RGBufferViewHandle CreateStorageBufferView(const RenderGraphDebugName& name, Uint64 size, const rhi::RHIAllocationInfo& allocationInfo = rhi::EMemoryUsage::GPUOnly);

	void ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination);
	
	// Utilities ==============================================

	RenderGraphResourcesPool& GetResourcesPool() const;

	template<typename TType, typename... TArgs>
	TType* Allocate(TArgs&&... args);

	RGAllocator& GetAllocator() { return m_allocator; }

	lib::MemoryArena& GetMemoryArena() { return m_memoryArena; }

	// Diagnostics ============================================

#if RG_ENABLE_DIAGNOSTICS
	void PushProfilerScope(lib::HashedString name);
	void PopProfilerScope();
#endif // RG_ENABLE_DIAGNOSTICS
	
	// Commands ===============================================

	/** Calls dispatch command with given params */
	template<typename TShaderParams = EmptyShaderParams>
	void Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, const TShaderParams& params = TShaderParams{});

	/** Calls dispatch indirect command with given shader params */
	template<typename TShaderParams = EmptyShaderParams>
	void DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, const TShaderParams& shaderParams = TShaderParams{});

	/** Creates render pass with given shader params and executes callable inside it */
	template<typename TShaderParams, typename TCallable>
	void RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, const TShaderParams& shaderParams, TCallable&& callable);

	/** Creates render pass with given shader params and executes callable inside it */
	template<typename TShaderParams, typename TPassParameters, typename TCallable>
	void RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, const TShaderParams& shaderParams, const TPassParameters& parameters, TCallable&& callable);

	/** Creates render pass with given descriptor sets and executes full screen triangle draw call inside it */
	template<typename TShaderParams = EmptyShaderParams>
	void FullScreenPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, rdr::PipelineStateID pipelineID, const TShaderParams& shaderParams = TShaderParams{});

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TShaderParams, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, const TShaderParams& shaderParams, TCallable&& callable);

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TShaderParams, typename TPassParameters, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, const TShaderParams& shaderParams, const TPassParameters& parameters, TCallable&& callable);

	template<typename TShaderParams = EmptyShaderParams>
	void TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, const TShaderParams& shaderParams = TShaderParams{});

	template<typename TShaderParams = EmptyShaderParams>
	void TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, const TShaderParams& shaderParams = TShaderParams{});

	template<typename TPassParameters, typename TCallable>
	void AddLambdaPass(const RenderGraphDebugName& passName, const TPassParameters& parameters, TCallable&& callable);

	void BuildBLASes(const RenderGraphDebugName& commandName, lib::Span<const BLASBuildCommand> buildCommands);

	void BuildTLAS(const RenderGraphDebugName& commandName, const TLASBuildCommand& buildCommand);

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

	template<typename TDataType>
	rdr::GPUPtr<TDataType> CreateGPUData(const TDataType& data);

	template<typename TDataType>
	rdr::GPUPtr<TDataType> CreateGPUData();

	template<typename TShaderParam>
	void BindShaderParam(const TShaderParam& param);;

	template<typename TShaderParam>
	void UnbindShaderParam();

	void UnbindShaderParam(const lib::HashedString& paramType);

	const js::Event& GetGPUFinishedEvent() const         { return m_onGraphExecutionFinished; }
	const js::Event& GetPreGPUWorkSubmittedEvent() const { return m_preGPUWorkSubmittedEvent; }

	void Execute();

private:

	friend struct GenericShaderParamsPtr;

	template<typename TNodeType, typename... TArgs>
	TNodeType& AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args);

	template<typename TCallable>
	RGNode& CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TCallable&& callable);

	template<typename TParametersTuple>
	void BuildParametersDependencies(const TParametersTuple& parametersStructs, RGDependenciesBuilder& dependenciesBuilder) const;
	
	template<typename TParameters>
	void BuildParametersStructDependencies(const TParameters& parameters, RGDependenciesBuilder& dependenciesBuilder) const;

	template<typename TShaderParam>
	void AssignShaderParamToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParam& shaderParam, RGDependenciesBuilder& dependenciesBuilder);

	template<typename TShaderParams>
	void AssignShaderParamsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParams& shaderParams, RGDependenciesBuilder& dependenciesBuilder);

	template<typename TShaderParam>
	void AssignShaderParamToSubpass(RGSubpass& subpass, const TShaderParam& shaderParam, RGDependenciesBuilder& dependenciesBuilder);

	Bool AssignShaderParamToNodeInternal(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<const Byte> paramData, const lib::HashedString& paramType, RGDependenciesBuilder& dependenciesBuilder);
	Bool AssignShaderParamToNodeInternal(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const lib::SharedPtr<rdr::BindableBufferView>& bufferView, Uint32 offset, Uint32 size, const lib::HashedString& paramType, RGDependenciesBuilder& dependenciesBuilder);

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

	rdr::ConstantBufferAllocation AllocateConstantBuffer(lib::Span<const Byte> data);
	rdr::ConstantBufferAllocation AllocateConstantBuffer(Uint32 size);

	rdr::PipelineStateID GetOrCreateComputePipelineStateID(rdr::ShaderID shader) const;

	lib::SharedPtr<rdr::Pipeline> GetPipelineObject(rdr::PipelineStateID psoID) const;

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

	struct BoundShaderParam
	{
		lib::HashedString paramType;
		lib::Span<const Byte> data;
		lib::RawCallable<void(rg::RenderGraphBuilder&, rg::RGNode&, const lib::SharedPtr<rdr::Pipeline>&, lib::Span<const Byte>, rg::RGDependenciesBuilder&)> binder;
	};

	lib::InlineDynamicArray<BoundShaderParam, 32u> m_boundShaderParams;

	lib::InlineDynamicArray<lib::SharedPtr<RenderGraphDebugDecorator>, 4u> m_debugDecorators;

	lib::SharedPtr<rdr::GPUStatisticsCollector> m_statisticsCollector;

	RGResourceHandle<RGRenderPassNodeBase> m_lastRenderPassNode;

	lib::DynamicArray<RGTextureViewHandle> m_pendingGloballyReadableTransitions;

	js::Event m_onGraphExecutionFinished;
	js::Event m_preGPUWorkSubmittedEvent;

	RenderGraphResourcesPool& m_resourcesPool;

#if RG_ENABLE_DIAGNOSTICS
	RGProfilerRecorder m_profilerRecorder;
#endif // RG_ENABLE_DIAGNOSTICS

	RGAllocator m_allocator;

	lib::MemoryArena& m_memoryArena;
};

template<typename TType, typename... TArgs>
TType* RenderGraphBuilder::Allocate(TArgs&&... args)
{
	return m_allocator.Allocate<TType>(std::forward<TArgs>(args)...);
}

template<typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::Dispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const WorkloadResolution& groupCount, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const auto executeLambda = [computePipelineID, groupCount](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
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

	AssignShaderParamsToNode(node, GetPipelineObject(computePipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::DispatchIndirect(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const auto executeLambda = [computePipelineID, indirectArgsBuffer, indirectArgsOffset](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
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

	dependenciesBuilder.AddBufferAccess(indirectArgsBuffer, ERGBufferAccess::Read, rhi::EPipelineStage::DrawIndirect);

	AssignShaderParamsToNode(node, GetPipelineObject(computePipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TShaderParams, typename TCallable>
void RenderGraphBuilder::RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, const TShaderParams& shaderParams, TCallable&& callable)
{
	RenderPass(renderPassName, renderPassDef, shaderParams, std::make_tuple(), callable);
}

template<typename TShaderParams, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::RenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, const TShaderParams& shaderParams, const TPassParameters& parameters, TCallable&& callable)
{
	RGNode& node = CreateRenderPassNodeInternal(renderPassName, renderPassDef, std::forward<TCallable>(callable));

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);
	
	renderPassDef.BuildDependencies(dependenciesBuilder);
	BuildParametersDependencies(parameters, dependenciesBuilder);

	AssignShaderParamsToNode(node, nullptr, shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::FullScreenPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, rdr::PipelineStateID pipelineID, const TShaderParams& shaderParams /* = TShaderParams{} */)
{
	const math::Vector2u resolution = renderPassDef.GetRenderAreaExtent();

	auto callable = [resolution, pipelineID](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.SetViewport(math::AlignedBox2f(math::Vector2f(0.f, 0.f), resolution.cast<Real32>()), 0.f, 1.f);
		recorder.SetScissor(math::AlignedBox2u(math::Vector2u(0, 0), resolution));

		recorder.BindGraphicsPipeline(pipelineID);

		recorder.DrawInstances(3u, 1u);
	};

	using CallableType = std::remove_cvref_t<decltype(callable)>;

	RGNode& node = CreateRenderPassNodeInternal(renderPassName, renderPassDef, std::forward<CallableType>(callable));

	RGDependeciesContainer dependencies(m_memoryArena);
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);

	const lib::SharedPtr<rdr::Pipeline> pipeline = GetPipelineObject(pipelineID);

	AssignShaderParamsToNode(node, pipeline, shaderParams, dependenciesBuilder);
	
	renderPassDef.BuildDependencies(dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TShaderParams, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, const TShaderParams& shaderParams, TCallable&& callable)
{
	AddSubpass(subpassName, shaderParams, std::make_tuple(), std::forward<TCallable>(callable));
}

template<typename TShaderParams, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, const TShaderParams& shaderParams, const TPassParameters& parameters, TCallable&& callable)
{
	SPT_CHECK(!!m_lastRenderPassNode);

	using CallableType = std::remove_cvref_t<TCallable>;
	using SubpassType  = RGLambdaSubpass<CallableType>;
	RGSubpassHandle subpass = m_memoryArena.AllocateType<SubpassType>(m_memoryArena, subpassName, std::forward<TCallable>(callable));

	m_lastRenderPassNode->AppendSubpass(subpass);

	RGDependeciesContainer subpassDependencies(m_memoryArena);
	RGDependenciesBuilder subpassDependenciesBuilder(*this, subpassDependencies, rhi::EPipelineStage::ALL_GRAPHICS_SHADERS);
	
	BuildParametersDependencies(parameters, subpassDependenciesBuilder);

	if constexpr (!std::is_same_v<TShaderParams, EmptyShaderParams>)
	{
		if constexpr (lib::isTuple<TShaderParams>)
		{
			std::apply([&](const auto&... params)
			{
				(AssignShaderParamToSubpass(*subpass, params, subpassDependenciesBuilder), ...);
			}, shaderParams);
		}
		else
		{
			AssignShaderParamToSubpass(subpass, shaderParams, subpassDependenciesBuilder);
		}
	}

	ResolveNodeDependecies(*m_lastRenderPassNode, subpassDependencies);

	PostSubpassAdded(*m_lastRenderPassNode, subpassDependencies);
}

template<typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::TraceRays(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, const WorkloadResolution& traceCount, const TShaderParams& shaderParams /* = TShaderParams{} */)
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

	AssignShaderParamsToNode(node, GetPipelineObject(rayTracingPipelineID), shaderParams, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TShaderParams /* = EmptyShaderParams */>
void RenderGraphBuilder::TraceRaysIndirect(const RenderGraphDebugName& traceName, rdr::PipelineStateID rayTracingPipelineID, RGBufferViewHandle indirectArgsBuffer, Uint64 indirectArgsOffset, const TShaderParams& shaderParams /* = TShaderParams{} */)
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

template<typename TDataType>
rdr::GPUPtr<TDataType> RenderGraphBuilder::CreateGPUData(const TDataType& data)
{
	const rdr::HLSLStorage<TDataType> hlslData = data;

	const rdr::ConstantBufferAllocation cbAllocation = AllocateConstantBuffer(hlslData.GetHLSLDataSpan());

	return rdr::GPUPtr<TDataType>(cbAllocation.buffer->GetFullView(), cbAllocation.offset);
}

template<typename TDataType>
rdr::GPUPtr<TDataType> RenderGraphBuilder::CreateGPUData()
{
	const rdr::ConstantBufferAllocation cbAllocation = AllocateConstantBuffer(sizeof(rdr::HLSLStorage<TDataType>));
	return rdr::GPUPtr<TDataType>(cbAllocation.buffer->GetFullView(), cbAllocation.offset);
}

template<typename TShaderParam>
void RenderGraphBuilder::BindShaderParam(const TShaderParam& param)
{
	if constexpr (rdr::isGPUPtr<TShaderParam>)
	{
		using TShaderParamsStruct = typename TShaderParam::DataType;
		
		struct PtrWrapper
		{
			RGBufferViewHandle buffer;
			Uint32 offset = 0u;
		};

		PtrWrapper* data = GetMemoryArena().AllocateType<PtrWrapper>();
		data->buffer = AcquireExternalBufferView(param.GetBufferView());
		data->offset = param.GetOffset();

		BoundShaderParam boundParam;
		boundParam.paramType = TShaderParamsStruct::GetStructName();
		boundParam.data      = lib::Span<const Byte>(reinterpret_cast<const Byte*>(data), sizeof(PtrWrapper));
		boundParam.binder    = [](rg::RenderGraphBuilder& graphBuilder, rg::RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<const Byte> data, rg::RGDependenciesBuilder& dependenciesBuilder)
		{
			const PtrWrapper& shaderParam = *reinterpret_cast<const PtrWrapper*>(data.data());
			const rdr::GPUPtr<TShaderParamsStruct> gpuPtr(shaderParam.buffer->GetResource(), shaderParam.offset);
			graphBuilder.AssignShaderParamToNode<rdr::GPUPtr<TShaderParamsStruct>>(node, pipeline, gpuPtr, dependenciesBuilder);
		};

		SPT_CHECK(lib::ContainsPred(m_boundShaderParams, [&](const BoundShaderParam& boundParam) { return boundParam.paramType.GetView() == TShaderParamsStruct::GetStructName(); }) == false);

		m_boundShaderParams.EmplaceBack(std::move(boundParam));
	}
	else
	{
		rdr::HLSLStorage<TShaderParam>* data = GetMemoryArena().AllocateType<rdr::HLSLStorage<TShaderParam>>();
		*data = param;

		BoundShaderParam boundParam;
		boundParam.paramType = TShaderParam::GetStructName();
		boundParam.data      = data->GetHLSLDataSpan();
		boundParam.binder    = [](rg::RenderGraphBuilder& graphBuilder, rg::RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<const Byte> data, rg::RGDependenciesBuilder& dependenciesBuilder)
		{
			const rdr::HLSLStorage<TShaderParam>& shaderParam = *reinterpret_cast<const rdr::HLSLStorage<TShaderParam>*>(data.data());
			graphBuilder.AssignShaderParamToNode(node, pipeline, shaderParam, dependenciesBuilder);
		};

		SPT_CHECK(lib::ContainsPred(m_boundShaderParams, [&](const BoundShaderParam& boundParam) { return boundParam.paramType.GetView() == TShaderParam::GetStructName(); }) == false);

		m_boundShaderParams.EmplaceBack(std::move(boundParam));
	}
}

template<typename TShaderParam>
void RenderGraphBuilder::UnbindShaderParam()
{
	if constexpr (rdr::isGPUPtr<TShaderParam>)
	{
		using TShaderParamsStruct = typename TShaderParam::DataType;
		UnbindShaderParam<TShaderParamsStruct>();
	}
	else
	{
		UnbindShaderParam(TShaderParam::GetStructName());
	}
}

template<typename TNodeType, typename... TArgs>
TNodeType& RenderGraphBuilder::AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args)
{
	const RGNodeID nodeID = m_nodeCounter++;

	TNodeType* allocatedNode = m_memoryArena.AllocateType<TNodeType>(*this, name, nodeID, type, std::forward<TArgs>(args)...);
	SPT_CHECK(!!allocatedNode);

	return *allocatedNode;
}

template<typename TCallable>
RGNode& RenderGraphBuilder::CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TCallable&& callable)
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

template<typename TShaderParam>
void RenderGraphBuilder::AssignShaderParamToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParam& shaderParam, RGDependenciesBuilder& dependenciesBuilder)
{
	if constexpr (!std::is_same_v<TShaderParam, EmptyShaderParams>)
	{
		if constexpr (std::is_same_v<TShaderParam, GenericShaderParamsPtr>)
		{
			if (shaderParam.IsValid())
			{
				shaderParam.AssignToNode(*this, node, pipeline, dependenciesBuilder);
			}
		}
		else if constexpr (rdr::isGPUPtr<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::DataType;

			if (shaderParam.IsValid())
			{
				const Bool assignedParams = AssignShaderParamToNodeInternal(node, pipeline, shaderParam.GetBufferView(), shaderParam.GetOffset(), shaderParam.GetSize(), TShaderParamsStruct::GetStructName(), dependenciesBuilder);

				if (assignedParams)
				{
					rg::CollectStructDependencies<TShaderParamsStruct>(shaderParam.GetHLSLDataSpan(), dependenciesBuilder);
				}
			}
		}
		else if constexpr (rdr::isHLSLStorage<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::Struct;

			const Bool assignedParams = AssignShaderParamToNodeInternal(node, pipeline, shaderParam.GetHLSLDataSpan(), TShaderParamsStruct::GetStructName(), dependenciesBuilder);

			if (assignedParams)
			{
				rg::CollectStructDependencies<TShaderParamsStruct>(shaderParam.GetHLSLDataSpan(), dependenciesBuilder);
			}
		}
		else
		{
			const rdr::HLSLStorage<TShaderParam> shaderParamHLSLData = shaderParam;

			const Bool assignedParams = AssignShaderParamToNodeInternal(node, pipeline, shaderParamHLSLData.GetHLSLDataSpan(), TShaderParam::GetStructName(), dependenciesBuilder);

			if (assignedParams)
			{
				rg::CollectStructDependencies<TShaderParam>(shaderParamHLSLData.GetHLSLDataSpan(), dependenciesBuilder);
			}
		}
	}
}

template<typename TShaderParams>
void RenderGraphBuilder::AssignShaderParamsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const TShaderParams& shaderParams, RGDependenciesBuilder& dependenciesBuilder)
{
	if constexpr (!std::is_same_v<TShaderParams, EmptyShaderParams>)
	{
		if constexpr (lib::isTuple<TShaderParams>)
		{
			std::apply([&](const auto&... params)
			{
				(AssignShaderParamToNode(node, pipeline, params, dependenciesBuilder), ...);
			}, shaderParams);
		}
		else
		{
			AssignShaderParamToNode(node, pipeline, shaderParams, dependenciesBuilder);
		}
	}

	for (const BoundShaderParam& boundShaderParam : m_boundShaderParams)
	{
		boundShaderParam.binder(*this, node, pipeline, boundShaderParam.data, dependenciesBuilder);
	}
}

template<typename TShaderParam>
void RenderGraphBuilder::AssignShaderParamToSubpass(RGSubpass& subpass, const TShaderParam& shaderParam, RGDependenciesBuilder& dependenciesBuilder)
{
	if constexpr (!std::is_same_v<TShaderParam, EmptyShaderParams>)
	{
		if constexpr (std::is_same_v<TShaderParam, GenericShaderParamsPtr>)
		{
			if (shaderParam.IsValid())
			{
				shaderParam.AssignToSubpass(*this, subpass, dependenciesBuilder);
			}
		}
		else if constexpr (rdr::isGPUPtr<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::DataType;

			if (shaderParam.IsValid())
			{
				subpass.AddShaderParam(TShaderParamsStruct::GetStructName(), shaderParam.GetDeviceAddress());

				rg::CollectStructDependencies<TShaderParamsStruct>(shaderParam.GetHLSLDataSpan(), dependenciesBuilder);
			}
		}
		else if constexpr (rdr::isHLSLStorage<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::Struct;

			const rdr::GPUPtr<TShaderParam> shaderParamPtr = CreateGPUData(shaderParam);

			subpass.AddShaderParam(TShaderParamsStruct::GetStructName(), shaderParamPtr.GetDeviceAddress());

			rg::CollectStructDependencies<TShaderParamsStruct>(shaderParam.GetHLSLDataSpan(), dependenciesBuilder);
		}
		else
		{
			const rdr::HLSLStorage<TShaderParam> shaderParamHLSLData = shaderParam;

			const rdr::GPUPtr<TShaderParam> shaderParamPtr = CreateGPUData(shaderParam);

			subpass.AddShaderParam(TShaderParam::GetStructName(), shaderParamPtr.GetDeviceAddress());

			rg::CollectStructDependencies<TShaderParam>(shaderParamHLSLData.GetHLSLDataSpan(), dependenciesBuilder);
		}
	}
}

template<typename TShaderParams>
GenericShaderParamsPtr::GenericShaderParamsPtr(const rdr::GPUPtr<TShaderParams>& shaderParams)
	: m_data(shaderParams)
{
	static_assert(!std::is_same_v<TShaderParams, void>);

	m_nodeBinder = [](RenderGraphBuilder& graphBuilder, RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, const rdr::GPUPtr<void>& data, RGDependenciesBuilder& dependenciesBuilder)
	{
		const rdr::GPUPtr<TShaderParams> typedData(data.GetBufferView(), data.GetOffset());
		graphBuilder.AssignShaderParamToNode(node, pipeline, typedData, dependenciesBuilder);
	};

	m_subpassBinder = [](RenderGraphBuilder& graphBuilder, RGSubpass& subpass, const rdr::GPUPtr<void>& data, RGDependenciesBuilder& dependenciesBuilder)
	{
		const rdr::GPUPtr<TShaderParams> typedData(data.GetBufferView(), data.GetOffset());
		graphBuilder.AssignShaderParamToSubpass(subpass, typedData, dependenciesBuilder);
	};
}

inline void GenericShaderParamsPtr::AssignToNode(RenderGraphBuilder& graphBuilder, RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, RGDependenciesBuilder& dependenciesBuilder) const
{
	SPT_CHECK(m_nodeBinder.IsValid());
	m_nodeBinder(graphBuilder, node, pipeline, m_data, dependenciesBuilder);
}

inline void GenericShaderParamsPtr::AssignToSubpass(RenderGraphBuilder& graphBuilder, RGSubpass& subpass, RGDependenciesBuilder& dependenciesBuilder) const
{
	SPT_CHECK(m_subpassBinder.IsValid());
	m_subpassBinder(graphBuilder, subpass, m_data, dependenciesBuilder);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Render Graph Utilities ========================================================================

template<typename ShaderParams>
class BindShaderParamsScope
{
public:

	BindShaderParamsScope(RenderGraphBuilder& graphBuilder, const ShaderParams& descriptorSets)
		: m_graphBuilder(graphBuilder)
	{
		std::apply([this](const auto&... params)
		{
			(m_graphBuilder.BindShaderParam(params), ...);
		}, descriptorSets);
	}

	~BindShaderParamsScope()
	{
		UnbindHelper(std::make_index_sequence<std::tuple_size_v<ShaderParams>>{});
	}

private:

	template<typename TShaderParam>
	static lib::HashedString GetShaderParamTypeName()
	{
		if constexpr (rdr::isGPUPtr<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::DataType;
			return TShaderParamsStruct::GetStructName();
		}
		else if constexpr (rdr::isHLSLStorage<TShaderParam>)
		{
			using TShaderParamsStruct = typename TShaderParam::Struct;
			return TShaderParamsStruct::GetStructName();
		}
		else
		{
			return TShaderParam::GetStructName();
		}
	}

	template<size_t... Is>
	void UnbindHelper(std::index_sequence<Is...>)
	{
		(m_graphBuilder.UnbindShaderParam(
			GetShaderParamTypeName<std::decay_t<std::tuple_element_t<Is, ShaderParams>>>()
		), ...);
	}

	RenderGraphBuilder& m_graphBuilder;
};

} // spt::rg
