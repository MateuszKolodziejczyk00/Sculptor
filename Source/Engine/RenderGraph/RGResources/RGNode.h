#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "RGResourceHandles.h"
#include "RGRenderPassDefinition.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RenderGraphTypes.h"
#include "Pipelines/PipelineState.h"
#include "RGDiagnostics.h"
#include "RHIBridge/RHIDependencyImpl.h"


namespace spt::rhi
{
struct BarrierTextureTransitionDefinition;
} // spt::rhi


namespace spt::rdr
{
class RenderContext;
class CommandRecorder;
class GPUStatisticsCollector;
} // spt::rdr


namespace spt::rg
{

using RGNodeID = SizeType;
class RenderGraphBuilder;


struct RGExecutionContext
{
	lib::SharedPtr<rdr::GPUStatisticsCollector> statisticsCollector;
};


struct RGNodeNullDebugMetaData { };

struct RGNodeComputeDebugMetaData
{
	rdr::PipelineStateID pipelineStateID;
};

using RGNodeDebugMetaData = std::variant<RGNodeNullDebugMetaData, RGNodeComputeDebugMetaData>;


class RENDER_GRAPH_API RGNode : public RGTrackedObject
{
public:

	RGNode(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type);

	RenderGraphBuilder&			GetOwningGraphBuilder() const;
	RGNodeID					GetID() const;
	const RenderGraphDebugName& GetName() const;
	ERenderGraphNodeType		GetType() const;

#if DEBUG_RENDER_GRAPH
	void                       SetDebugMetaData(RGNodeDebugMetaData metaData);
	const RGNodeDebugMetaData& GetDebugMetaData() const;
#endif // DEBUG_RENDER_GRAPH

#if RG_ENABLE_DIAGNOSTICS
	void SetDiagnosticsRecord(RGDiagnosticsRecord record);
	const RGDiagnosticsRecord& GetDiagnosticsRecord() const;
#endif // RG_ENABLE_DIAGNOSTICS

	void AddTextureToAcquire(RGTextureHandle texture);
	void AddTextureToRelease(RGTextureHandle texture);

	// texture views are not reused so they don't need to be released after acquire
	void AddTextureViewToAcquire(RGTextureViewHandle textureView);

	void AddBufferToAcquire(RGBufferHandle buffer);
	void AddBufferToRelease(RGBufferHandle buffer);

	void AddPreExecutionBarrier(rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess);

	void AddDescriptorSetState(lib::MTHandle<rdr::DescriptorSetState> dsState);

	void SetShaderParamsDescriptors(const rhi::RHIDescriptorRange& range);

	void Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context);

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context) = 0;

private:

	// Node Execution ===================================================

	void AcquireResources();
	void PreExecuteBarrier(rdr::CommandRecorder& recorder);
	void ReleaseResources();

	void BindDescriptorSetStates(rdr::CommandRecorder& recorder);
	void BindShaderParams(rdr::CommandRecorder& recorder);
	void UnbindDescriptorSetStates(rdr::CommandRecorder& recorder);

	// Execution Helpers ================================================

	void AcquireTextures();
	void ReleaseTextures();

	void AcquireTextureViews();
	
	void AcquireBuffers();
	void ReleaseBuffers();

	RenderGraphBuilder& m_owningGraphBuilder;

	RenderGraphDebugName m_name;

	RGNodeID m_id;

	ERenderGraphNodeType m_type;

	lib::DynamicArray<RGTextureHandle> m_texturesToAcquire;
	lib::DynamicArray<RGTextureHandle> m_texturesToRelease;

	lib::DynamicArray<RGBufferHandle> m_buffersToAcquire;
	lib::DynamicArray<RGBufferHandle> m_buffersToRelease;
	
	lib::DynamicArray<RGTextureViewHandle> m_textureViewsToAcquire;

	rhi::RHIDependency m_preExecuteDependency;

	lib::DynamicArray<lib::MTHandle<rdr::DescriptorSetState>> m_dsStates;

	Uint32 m_shaderParamsDescriptorHeapOffset = idxNone<Uint32>;

#if DEBUG_RENDER_GRAPH
	RGNodeDebugMetaData m_debugMetaData;
#endif // DEBUG_RENDER_GRAPH

#if RG_ENABLE_DIAGNOSTICS
	RGDiagnosticsRecord m_diagnosticsRecord;
#endif // RG_ENABLE_DIAGNOSTICS

	Bool m_executed;
};


template<typename TCallable>
class RGLambdaNode : public RGNode
{
protected:

	using Super = RGNode;

public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedRef<rdr::RenderContext>&, rdr::CommandRecorder&>));

	explicit RGLambdaNode(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, TCallable callable)
		: Super(owningGraphBuilder, name, id, type)
		, m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context) override
	{
		m_callable(renderContext, recorder);
	}

private:

	TCallable m_callable;
};


class RENDER_GRAPH_API RGSubpass : public RGTrackedObject
{
public:

	explicit RGSubpass(const RenderGraphDebugName& name);
	
	const RenderGraphDebugName& GetName() const;

	void BindDSState(lib::MTHandle<rdr::DescriptorSetState> ds);

	void Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context);

protected:

	virtual void DoExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) = 0;

private:

	lib::DynamicArray<lib::MTHandle<rdr::DescriptorSetState>> m_dsStatesToBind;

	RenderGraphDebugName m_name;
};


template<typename TCallable>
class RGLambdaSubpass : public RGSubpass
{
protected:

	using Super = RGSubpass;

public:

	explicit RGLambdaSubpass(const RenderGraphDebugName& name, TCallable callable)
		: Super(name)
		, m_callable(callable)
	{ }

protected:

	virtual void DoExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) override
	{
		m_callable(renderContext, recorder);
	}

private:

	TCallable				m_callable;
};


class RENDER_GRAPH_API RGRenderPassNodeBase : public RGNode
{
protected:

	using Super = RGNode;

public:

	explicit RGRenderPassNodeBase(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, const RGRenderPassDefinition& renderPassDef);

	void AppendSubpass(RGSubpassHandle subpass);

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context) final;

	virtual void ExecuteRenderPass(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) = 0;

private:

	RGRenderPassDefinition m_renderPassDef;

	lib::DynamicArray<RGSubpassHandle> m_subpasses;
};


template<typename TCallable>
class RGRenderPassNode : public RGRenderPassNodeBase
{
protected:

	using Super = RGRenderPassNodeBase;

public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedRef<rdr::RenderContext>&, rdr::CommandRecorder&>));

	explicit RGRenderPassNode(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, const RGRenderPassDefinition& renderPassDef, TCallable callable)
		: Super(owningGraphBuilder, name, id, type, renderPassDef)
		, m_callable(std::move(callable))
	{ }

protected:

	virtual void ExecuteRenderPass(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) override
	{
		m_callable(renderContext, recorder);
	}

private:

	TCallable m_callable;
};


class RGEmptyNode : public RGNode
{
protected:

	using Super = RGNode;

public:

	RGEmptyNode(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type)
		: Super(owningGraphBuilder, name, id, type)
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context) override
	{ }
};

} // spt::rg
