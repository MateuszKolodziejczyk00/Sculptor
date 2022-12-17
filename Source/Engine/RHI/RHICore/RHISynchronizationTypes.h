#pragma once

#include "SculptorCoreTypes.h"
#include "RHITextureTypes.h"


namespace spt::rhi
{

struct BarrierTextureTransitionTarget
{
	constexpr BarrierTextureTransitionTarget()
		: accessType(EAccessType::None)
		, layout(ETextureLayout::Undefined)
		, stage(EPipelineStage::None)
	{ }

	constexpr BarrierTextureTransitionTarget(EAccessType inAccessType, ETextureLayout inLayout, EPipelineStage inStage)
		: accessType(inAccessType)
		, layout(inLayout)
		, stage(inStage)
	{ }

	EAccessType				accessType;
	ETextureLayout			layout;
	EPipelineStage			stage;
};


namespace TextureTransition
{

	static constexpr BarrierTextureTransitionTarget Undefined			= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::Undefined, EPipelineStage::TopOfPipe);

	// This can be used only as a source transition
	static constexpr BarrierTextureTransitionTarget Auto				= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::Auto, EPipelineStage::None);

	static constexpr BarrierTextureTransitionTarget ComputeGeneral		= BarrierTextureTransitionTarget(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::ComputeShader);
	static constexpr BarrierTextureTransitionTarget FragmentGeneral		= BarrierTextureTransitionTarget(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::FragmentShader);

	static constexpr BarrierTextureTransitionTarget FragmentReadOnly	= BarrierTextureTransitionTarget(EAccessType::Read, ETextureLayout::ColorReadOnlyOptimal, EPipelineStage::FragmentShader);
	
	static constexpr BarrierTextureTransitionTarget PresentSource		= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::PresentSrc, EPipelineStage::TopOfPipe);
	
	static constexpr BarrierTextureTransitionTarget ColorRenderTarget	= BarrierTextureTransitionTarget(EAccessType::Write, ETextureLayout::ColorRTOptimal, EPipelineStage::ColorRTOutput);

}


enum class EEventFlags
{
	None	= 0,
	GPUOnly	= BIT(0)
};


struct EventDefinition
{
	explicit EventDefinition(EEventFlags inFlags = EEventFlags::None)
		: flags(inFlags)
	{ }
	
	EEventFlags flags;
};

} // spt::rhi