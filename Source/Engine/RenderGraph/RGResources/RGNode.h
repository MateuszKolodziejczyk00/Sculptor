#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RGResources/RGTrackedObject.h"
#include "RGResources/RGResources.h"

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

	RGNode(const RenderGraphDebugName& name, RGNodeID id);

	RGNodeID GetID() const;
	const RenderGraphDebugName& GetName() const;

	void AddTextureToAcquire(RGTextureHandle texture);
	void AddTextureToRelease(RGTextureHandle texture);

	void AddTextureState(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	void Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, const lib::SharedPtr<rdr::CommandRecorder>& recorder);

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, const lib::SharedPtr<rdr::CommandRecorder>& recorder) = 0;

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

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedRef<rdr::RenderContext>&, const lib::SharedPtr<rdr::CommandRecorder>&>));

	explicit RGLambdaNode(const RenderGraphDebugName& name, RGNodeID id, TCallable callable)
		: Super(name, id)
		, m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, const lib::SharedPtr<rdr::CommandRecorder>& recorder) override
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

	RGEmptyNode(const RenderGraphDebugName& name, RGNodeID id)
		: Super(name, id)
	{ }

protected:

	virtual void OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, const lib::SharedPtr<rdr::CommandRecorder>& recorder) override
	{ }
};

} // spt::rg
