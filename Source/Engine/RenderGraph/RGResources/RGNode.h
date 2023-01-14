#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "RGResources/RGResources.h"
#include "RGRenderPassDefinition.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"


namespace spt::rhi
{
struct BarrierTextureTransitionDefinition;
} // spt::rhi


namespace spt::rdr
{
class RenderContext;
class CommandRecorder;
} // spt::rdr


namespace spt::rg
{

using RGNodeID = SizeType;


class RENDER_GRAPH_API RGNode : public RGTrackedObject
{
public:

	RGNode(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type);

	RGNodeID					GetID() const;
	const RenderGraphDebugName& GetName() const;
	ERenderGraphNodeType		GetType() const;

	void AddTextureToAcquire(RGTextureHandle texture);
	void AddTextureToRelease(RGTextureHandle texture);

	void AddBufferToAcquire(RGBufferHandle buffer);
	void AddBufferToRelease(RGBufferHandle buffer);

	void AddTextureTransition(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void AddBufferSynchronization(RGBufferHandle buffer, Uint64 offset, Uint64 size, rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess);
	void TryAppendBufferSynchronizationDest(RGBufferHandle buffer, Uint64 offset, Uint64 size, rhi::EPipelineStage destStage, rhi::EAccessType destAccess);

	void AddDescriptorSetState(const lib::SharedRef<rdr::DescriptorSetState>& dsState);

	void Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder);

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) = 0;

private:

	// Node Execution ===================================================

	void CreateResources();
	void PreExecuteBarrier(rdr::CommandRecorder& recorder);
	void ReleaseResources();

	void BindDescriptorSetStates(rdr::CommandRecorder& recorder);
	void UnbindDescriptorSetStates(rdr::CommandRecorder& recorder);

	// Execution Helpers ================================================

	void CreateTextures();
	void ReleaseTextures();
	
	void CreateBuffers();
	void ReleaseBuffers();

	struct TextureTransitionDef
	{
		TextureTransitionDef(RGTextureHandle inTexture,
							 const rhi::TextureSubresourceRange& inTextureSubresourceRange,
							 const rhi::BarrierTextureTransitionDefinition* inTransitionSource,
							 const rhi::BarrierTextureTransitionDefinition* inTransitionTarget)
			: texture(inTexture)
			, textureSubresourceRange(inTextureSubresourceRange)
			, transitionSource(inTransitionSource)
			, transitionTarget(inTransitionTarget)
		{ }

		RGTextureHandle texture;
		rhi::TextureSubresourceRange textureSubresourceRange;
		const rhi::BarrierTextureTransitionDefinition* transitionSource;
		const rhi::BarrierTextureTransitionDefinition* transitionTarget;
	};

	struct BufferTransitionDef
	{
		BufferTransitionDef(RGBufferHandle inBuffer, Uint64 inOffset, Uint64 inSize, rhi::EPipelineStage inSourceStage, rhi::EAccessType inSourceAccess, rhi::EPipelineStage inDestStage, rhi::EAccessType inDestAccess)
			: buffer(inBuffer)
			, offset(inOffset)
			, size(inSize)
			, sourceStage(inSourceStage)
			, sourceAccess(inSourceAccess)
			, destStage(inDestStage)
			, destAccess(inDestAccess)
		{ }

		RGBufferHandle		buffer;
		Uint64				offset;
		Uint64				size;

		rhi::EPipelineStage	sourceStage;
		rhi::EAccessType	sourceAccess;

		rhi::EPipelineStage	destStage;
		rhi::EAccessType	destAccess;
	};

	RenderGraphDebugName m_name;

	RGNodeID m_id;

	ERenderGraphNodeType m_type;

	lib::DynamicArray<RGTextureHandle> m_texturesToAcquire;
	lib::DynamicArray<RGTextureHandle> m_texturesToRelease;

	lib::DynamicArray<RGBufferHandle> m_buffersToAcquire;
	lib::DynamicArray<RGBufferHandle> m_buffersToRelease;

	lib::DynamicArray<TextureTransitionDef>	m_preExecuteTextureTransitions;
	lib::DynamicArray<BufferTransitionDef>	m_preExecuteBufferTransitions;

	lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> m_dsStates;

	Bool m_executed;
};


template<typename TCallable>
class RGLambdaNode : public RGNode
{
protected:

	using Super = RGNode;

public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedRef<rdr::RenderContext>&, rdr::CommandRecorder&>));

	explicit RGLambdaNode(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, TCallable callable)
		: Super(name, id, type)
		, m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) override
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

	void BindDSState(lib::SharedRef<rdr::DescriptorSetState> ds);

	void Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder);

protected:

	virtual void DoExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) = 0;

private:

	lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> m_dsStatesToBind;

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

	explicit RGRenderPassNodeBase(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, const RGRenderPassDefinition& renderPassDef);

	void AppendSubpass(RGSubpassHandle subpass);

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) final;

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

	explicit RGRenderPassNode(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, const RGRenderPassDefinition& renderPassDef, TCallable callable)
		: Super(name, id, type, renderPassDef)
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

	RGEmptyNode(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type)
		: Super(name, id, type)
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder) override
	{ }
};

} // spt::rg
