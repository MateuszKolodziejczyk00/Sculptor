#pragma once

#include "SculptorCoreTypes.h"
#include "RHITextureTypes.h"


namespace spt::rhi
{

struct BarrierTextureTransitionDefinition
{
	constexpr BarrierTextureTransitionDefinition()
		: accessType(EAccessType::None)
		, layout(ETextureLayout::Undefined)
		, stage(EPipelineStage::None)
	{ }

	constexpr BarrierTextureTransitionDefinition(EAccessType inAccessType, ETextureLayout inLayout, EPipelineStage inStage)
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

	static constexpr BarrierTextureTransitionDefinition Undefined					= BarrierTextureTransitionDefinition(EAccessType::None, ETextureLayout::Undefined, EPipelineStage::TopOfPipe);

	// This can be used only as a source transition
	static constexpr BarrierTextureTransitionDefinition Auto						= BarrierTextureTransitionDefinition(EAccessType::None, ETextureLayout::Auto, EPipelineStage::None);

	static constexpr BarrierTextureTransitionDefinition ComputeGeneral				= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::ComputeShader);
	static constexpr BarrierTextureTransitionDefinition FragmentGeneral				= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::FragmentShader);

	static constexpr BarrierTextureTransitionDefinition ReadOnly					= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::ColorReadOnlyOptimal, EPipelineStage::ALL_COMMANDS);
	static constexpr BarrierTextureTransitionDefinition FragmentReadOnly			= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::ColorReadOnlyOptimal, EPipelineStage::FragmentShader);
	static constexpr BarrierTextureTransitionDefinition ComputeReadOnly				= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::ColorReadOnlyOptimal, EPipelineStage::ComputeShader);
	
	static constexpr BarrierTextureTransitionDefinition PresentSource				= BarrierTextureTransitionDefinition(EAccessType::None, ETextureLayout::PresentSrc, EPipelineStage::TopOfPipe);
	
	static constexpr BarrierTextureTransitionDefinition ColorRenderTarget			= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::ColorRTOptimal, EPipelineStage::ColorRTOutput);
	static constexpr BarrierTextureTransitionDefinition DepthRenderTarget			= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::DepthRTOptimal, lib::Flags(EPipelineStage::EarlyFragmentTest, EPipelineStage::LateFragmentTest));
	static constexpr BarrierTextureTransitionDefinition DepthStencilRenderTarget	= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::DepthStencilRTOptimal, lib::Flags(EPipelineStage::EarlyFragmentTest, EPipelineStage::LateFragmentTest));

	static constexpr BarrierTextureTransitionDefinition TransferSource				= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::TransferSrcOptimal, EPipelineStage::Transfer);
	static constexpr BarrierTextureTransitionDefinition TransferDest				= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::TransferDstOptimal, EPipelineStage::Transfer);
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