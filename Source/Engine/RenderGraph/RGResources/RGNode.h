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

class RENDER_GRAPH_API RGNode : public RGTrackedResource
{
public:

	RGNode();

	void AddTextureState(RGTextureHandle inTexture, const rhi::TextureSubresourceRange& inTextureSubresourceRange, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget);

	void Execute(const lib::SharedPtr<rdr::CommandRecorder>& recorder);

protected:

	void PreExecuteBarrier(const lib::SharedPtr<rdr::CommandRecorder>& recorder);

	virtual void OnExecute(const lib::SharedPtr<rdr::CommandRecorder>& recorder) = 0;

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

	lib::DynamicArray<TextureTransitionDef> m_preExecuteTransitions;

	Bool m_executed;
};


template<typename TCallable>
class RGLambdaNode
{
public:

	SPT_STATIC_CHECK((std::invocable<TCallable&, const lib::SharedPtr<rdr::CommandRecorder>&>));

	explicit RGLambdaNode(TCallable callable)
		: m_callable(std::move(callable))
	{ }

protected:

	virtual void OnExecute(const lib::SharedPtr<rdr::CommandRecorder>& recorder) override
	{
		m_callable(recorder);
	}

private:

	TCallable m_callable;
};

} // spt::rg
