#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedResource.h"
#include "RGResources/RGResources.h"

namespace spt::rhi
{
struct BarrierTextureTransitionTarget;
} // spt::rhi


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::rg
{

using RGNodeID = SizeType;


class RENDER_GRAPH_API RGNode : public RGTrackedResource
{
public:

	RGNode(const RenderGraphDebugName& name, RGNodeID id);

	RGNodeID GetID() const;
	const RenderGraphDebugName& GetName() const;

	void AddTextureToAcquire(RGTextureHandle texture);
	void AddTextureToRelease(RGTextureHandle texture);

	void AddTextureState(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget);

	void Execute(const lib::SharedPtr<rdr::CommandRecorder>& recorder);

protected:

	virtual void OnExecute(const lib::SharedPtr<rdr::CommandRecorder>& recorder) = 0;

private:

	// Node Execution ===================================================

	void CreateResources();
	void PreExecuteBarrier(const lib::SharedPtr<rdr::CommandRecorder>& recorder);
	void ReleaseResources();

	// Execution Helpers ================================================

	void CreateTextures();

	void ReleaseTextures();

	struct TextureTransitionDef
	{
		TextureTransitionDef(RGTextureHandle inTexture,
							 const rhi::TextureSubresourceRange& inTextureSubresourceRange,
							 const rhi::BarrierTextureTransitionTarget* inTransitionSource,
							 const rhi::BarrierTextureTransitionTarget* inTransitionTarget)
			: texture(inTexture)
			, textureSubresourceRange(inTextureSubresourceRange)
			, transitionSource(inTransitionSource)
			, transitionTarget(inTransitionTarget)
		{ }

		RGTextureHandle texture;
		rhi::TextureSubresourceRange textureSubresourceRange;
		const rhi::BarrierTextureTransitionTarget* transitionSource;
		const rhi::BarrierTextureTransitionTarget* transitionTarget;
	};


	RenderGraphDebugName m_name;

	RGNodeID m_id;

	lib::DynamicArray<RGTextureHandle> m_texturesToAcquire;
	lib::DynamicArray<RGTextureHandle> m_texturesToRelease;

	lib::DynamicArray<TextureTransitionDef> m_preExecuteTransitions;

	Bool m_executed;
};


template<typename TCallable>
class RGLambdaNode : public RGNode
{
protected:

	using Super = RGNode;

public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedPtr<rdr::CommandRecorder>&>));

	explicit RGLambdaNode(const RenderGraphDebugName& name, RGNodeID id, TCallable callable)
		: Super(name, id)
		, m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedPtr<rdr::CommandRecorder>& recorder) override
	{
		m_callable(recorder);
	}

private:

	TCallable m_callable;
};


class RGEmptyNode : public RGNode
{
protected:

	using Super = RGNode;

public:

	RGEmptyNode(const RenderGraphDebugName& name, RGNodeID id)
		: Super(name, id)
	{ }

protected:

	virtual void OnExecute(const lib::SharedPtr<rdr::CommandRecorder>& recorder) override
	{ }
};

} // spt::rg
