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
#include "RGNodeParametersStruct.h"
#include "Utility/Templates/Overload.h"

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


inline decltype(auto) EmptyDescriptorSets()
{
	static lib::StaticArray<lib::SharedRef<rg::RGDescriptorSetStateBase>, 0> empty;
	return empty;
}


class RENDER_GRAPH_API RenderGraphBuilder
{
public:

	RenderGraphBuilder();

	// Textures ===============================================

	RGTextureHandle AcquireExternalTexture(const lib::SharedPtr<rdr::Texture>& texture);

	RGTextureViewHandle AcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView);

	RGTextureHandle CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags = ERGResourceFlags::Default);

	RGTextureViewHandle CreateTextureView(const RenderGraphDebugName& name, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition, ERGResourceFlags flags = ERGResourceFlags::Default);

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

	template<typename TParametersType>
	TParametersType& AllocateParametersStruct();
	
	// Commands ===============================================

	/** Calls dispatch command with given descriptor sets */
	template<typename TDescriptorSetStatesRange>
	void AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange);

	/** Creates render pass with given descriptor sets and executes callable inside it */
	template<typename TDescriptorSetStatesRange, typename TCallable>
	void AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	/** Creates render pass with given descriptor sets and executes callable inside it */
	template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
	void AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable);

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TDescriptorSetStatesRange, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	/** Appends callable with its dependencies to previous render pass (must be called after render pass) */
	template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
	void AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable);

	void AddFillBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range, Uint32 data);

	void BindDescriptorSetState(const lib::SharedRef<rdr::DescriptorSetState>& dsState);
	void UnbindDescriptorSetState(const lib::SharedRef<rdr::DescriptorSetState>& dsState);

	void Execute(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores);

private:

	template<typename TNodeType, typename... TArgs>
	TNodeType& AllocateNode(const RenderGraphDebugName& name, ERenderGraphNodeType type, TArgs&&... args);

	template<typename TDescriptorSetStatesRange, typename TCallable>
	RGNode& CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable);

	template<typename TDescriptorSetStatesRange>
	void AddDescriptorSetStatesToNode(RGNodeHandle node, const TDescriptorSetStatesRange& dsStatesRange);

	template<typename TDescriptorSetStatesRange>
	void BuildDescriptorSetDependencies(const TDescriptorSetStatesRange& dsStatesRange, RGDependenciesBuilder& dependenciesBuilder) const;

	template<typename TParametersTuple>
	void BuildParametersDependencies(const TParametersTuple& parametersStructs, RGDependenciesBuilder& dependenciesBuilder) const;
	
	template<typename TParameters>
	void BuildParametersStructDependencies(const TParameters& parameters, RGDependenciesBuilder& dependenciesBuilder) const;

	void AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies);

	void ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies);
	void AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies);

	const rhi::BarrierTextureTransitionDefinition& GetTransitionDefForAccess(RGNodeHandle node, ERGTextureAccess access) const;

	void GetSynchronizationParamsForBuffer(ERGBufferAccess lastAccess, rhi::EAccessType& outAccessType) const;

	Bool RequiresSynchronization(const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget) const;
	Bool RequiresSynchronization(RGBufferHandle buffer, ERGBufferAccess prevAccess, ERGBufferAccess nextAccess) const;

	void PostBuild();
	void ExecuteGraph(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores);

	void AddReleaseResourcesNode();

	void ResolveResourceReleases();
	void ResolveTextureReleases();
	void ResolveBufferReleases();

	lib::DynamicArray<RGTextureHandle> m_textures;
	lib::DynamicArray<RGBufferHandle> m_buffers;

	lib::HashMap<lib::SharedPtr<rdr::Texture>, RGTextureHandle> m_externalTextures;
	lib::HashMap<lib::SharedPtr<rdr::Buffer>, RGBufferHandle> m_externalBuffers;

	lib::DynamicArray<RGTextureHandle> m_extractedTextures;
	lib::DynamicArray<RGBufferHandle> m_extractedBuffers;

	lib::DynamicArray<RGNodeHandle> m_nodes;

	lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> m_boundDSStates;

	RGAllocator m_allocator;
};

template<typename TParametersType>
TParametersType& RenderGraphBuilder::AllocateParametersStruct()
{
	return *m_allocator.Allocate<TParametersType>();
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::AddDispatch(const RenderGraphDebugName& dispatchName, rdr::PipelineStateID computePipelineID, const math::Vector3u& groupCount, TDescriptorSetStatesRange&& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	const auto executeLambda = [computePipelineID, groupCount, dsStatesRange](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		recorder.BindComputePipeline(computePipelineID);
		recorder.Dispatch(groupCount);
	};

	using LambdaType = std::remove_cv_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(dispatchName, ERenderGraphNodeType::Dispatch, std::move(executeLambda));
	AddDescriptorSetStatesToNode(&node, dsStatesRange);

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	BuildDescriptorSetDependencies(dsStatesRange, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TCallable>
void RenderGraphBuilder::AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	AddRenderPass(renderPassName, renderPassDef, dsStatesRange, std::make_tuple(), callable);
}

template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::AddRenderPass(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();
	
	RGNode& node = CreateRenderPassNodeInternal(renderPassName, renderPassDef, dsStatesRange, callable);
	AddDescriptorSetStatesToNode(&node, dsStatesRange);

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	BuildDescriptorSetDependencies(dsStatesRange, dependenciesBuilder);
	renderPassDef.BuildDependencies(dependenciesBuilder);
	BuildParametersDependencies(parameters, dependenciesBuilder);

	AddNodeInternal(node, dependencies);
}

template<typename TDescriptorSetStatesRange, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	AddSubpass(subpassName, dsStatesRange, std::make_tuple(), callable);
}

	template<typename TDescriptorSetStatesRange, typename TPassParameters, typename TCallable>
void RenderGraphBuilder::AddSubpass(const RenderGraphDebugName& subpassName, TDescriptorSetStatesRange&& dsStatesRange, const TPassParameters& parameters, TCallable&& callable)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_nodes.empty());

	RGNodeHandle lastNode = m_nodes.back();
	SPT_CHECK_MSG(lastNode->GetType() == ERenderGraphNodeType::RenderPass, "Subpasses can be added only to render pass nodes");

	RGRenderPassNodeBase* lastRenderPass = static_cast<RGRenderPassNodeBase*>(lastNode.Get());

	RGDependeciesContainer subpassDependencies;
	RGDependenciesBuilder subpassDependenciesBuilder(*this, subpassDependencies);
	BuildDescriptorSetDependencies(dsStatesRange, subpassDependenciesBuilder);
	BuildParametersDependencies(parameters, subpassDependenciesBuilder);

	ResolveNodeDependecies(*lastRenderPass, subpassDependencies);
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

template<typename TDescriptorSetStatesRange, typename TCallable>
RGNode& RenderGraphBuilder::CreateRenderPassNodeInternal(const RenderGraphDebugName& renderPassName, const RGRenderPassDefinition& renderPassDef, TDescriptorSetStatesRange&& dsStatesRange, TCallable&& callable)
{
	using LambdaType = TCallable;
	using NodeType = RGLambdaNode<LambdaType>;

	return AllocateNode<NodeType>(renderPassName, ERenderGraphNodeType::RenderPass, renderPassDef, std::forward<TCallable>(callable));
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::AddDescriptorSetStatesToNode(RGNodeHandle node, const TDescriptorSetStatesRange& dsStatesRange)
{
	SPT_PROFILER_FUNCTION();

	for (const lib::SharedRef<rdr::DescriptorSetState>& state : dsStatesRange)
	{
		node->AddDescriptorSetState(state);
	}
	for (const lib::SharedRef<rdr::DescriptorSetState>& state : m_boundDSStates)
	{
		node->AddDescriptorSetState(state);
	}
}

template<typename TDescriptorSetStatesRange>
void RenderGraphBuilder::BuildDescriptorSetDependencies(const TDescriptorSetStatesRange& dsStatesRange, RGDependenciesBuilder& dependenciesBuilder) const
{
	SPT_PROFILER_FUNCTION();

	for (const lib::SharedPtr<RGDescriptorSetStateBase>& stateToBind : dsStatesRange)
	{
		stateToBind->BuildRGDependencies(dependenciesBuilder);
	}
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
											   dependenciesBuilder.AddBufferAccess(buffer, access, pipelineStages);
										   },
										   [&dependenciesBuilder](RGTextureViewHandle texture, ERGTextureAccess access, rhi::EPipelineStage pipelineStages)
										   {
											   dependenciesBuilder.AddTextureAccess(texture, access, pipelineStages);
										   }));
}

} // spt::rg
