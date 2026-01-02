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

	static constexpr BarrierTextureTransitionDefinition PresentSource				= BarrierTextureTransitionDefinition(EAccessType::None, ETextureLayout::PresentSrc, EPipelineStage::TopOfPipe);
	
	static constexpr BarrierTextureTransitionDefinition ColorRenderTarget			= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Write, EAccessType::Read), ETextureLayout::ColorRTOptimal, EPipelineStage::ColorRTOutput);
	static constexpr BarrierTextureTransitionDefinition DepthRenderTarget			= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Write, EAccessType::Read), ETextureLayout::DepthRTOptimal, lib::Flags(EPipelineStage::EarlyFragmentTest, EPipelineStage::LateFragmentTest));
	static constexpr BarrierTextureTransitionDefinition DepthStencilRenderTarget	= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::DepthStencilRTOptimal, lib::Flags(EPipelineStage::EarlyFragmentTest, EPipelineStage::LateFragmentTest));

	static constexpr BarrierTextureTransitionDefinition TransferSource				= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::TransferSrcOptimal, EPipelineStage::Transfer);
	static constexpr BarrierTextureTransitionDefinition TransferDest				= BarrierTextureTransitionDefinition(EAccessType::Write, ETextureLayout::TransferDstOptimal, EPipelineStage::Transfer);

	static constexpr BarrierTextureTransitionDefinition Generic					= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, lib::Flags(EPipelineStage::Transfer, rhi::EPipelineStage::FragmentShader, rhi::EPipelineStage::ComputeShader));

	static constexpr BarrierTextureTransitionDefinition ShaderRead				= BarrierTextureTransitionDefinition(EAccessType::Read, ETextureLayout::General, lib::Flags(EPipelineStage::Transfer, rhi::EPipelineStage::FragmentShader, rhi::EPipelineStage::ComputeShader));
	static constexpr BarrierTextureTransitionDefinition ShaderWrite				= BarrierTextureTransitionDefinition(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, lib::Flags(EPipelineStage::Transfer, rhi::EPipelineStage::FragmentShader, rhi::EPipelineStage::ComputeShader));
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