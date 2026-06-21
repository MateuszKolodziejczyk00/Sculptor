#pragma once

#include "SculptorCoreTypes.h"
#include <variant>

namespace spt::rhi
{

enum class EPipelineStage : Flags64
{
	None							= 0,
	TopOfPipe						= BIT64(0),
	DrawIndirect					= BIT64(1),
	VertexShader					= BIT64(2),
	TaskShader						= BIT64(3),
	MeshShader						= BIT64(4),
	FragmentShader					= BIT64(5),
	EarlyFragmentTest				= BIT64(6),
	LateFragmentTest				= BIT64(7),
	ColorRTOutput					= BIT64(8),
	ComputeShader					= BIT64(9),
	Transfer						= BIT64(10),
	BottomOfPipe					= BIT64(11),
	Copy							= BIT64(12),
	Blit							= BIT64(13),
	Clear							= BIT64(14),
	IndexInput						= BIT64(15),

	ASBuild							= BIT64(16),

	RayTracingShader				= BIT64(17),

	Host							= BIT64(18),

	ALL_GRAPHICS_SHADERS			= VertexShader | FragmentShader | TaskShader | MeshShader,

	ALL_GRAPHICS					= BIT64(50),
	ALL_TRANSFER					= BIT64(51),
	ALL_COMMANDS					= BIT64(52)
};


inline lib::String ToString(EPipelineStage stage)
{
	lib::String result = "|";

	if (lib::HasAnyFlag(stage, EPipelineStage::TopOfPipe))         { result += "TopOfPipe|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::DrawIndirect))      { result += "DrawIndirect|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::VertexShader))      { result += "VertexShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::TaskShader))        { result += "TaskShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::MeshShader))        { result += "MeshShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::FragmentShader))    { result += "FragmentShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::EarlyFragmentTest)) { result += "EarlyFragmentTest|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::LateFragmentTest))  { result += "LateFragmentTest|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::ColorRTOutput))     { result += "ColorRTOutput|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::ComputeShader))     { result += "ComputeShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::Transfer))          { result += "Transfer|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::BottomOfPipe))      { result += "BottomOfPipe|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::Copy))              { result += "Copy|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::Blit))              { result += "Blit|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::Clear))             { result += "Clear|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::IndexInput))        { result += "IndexInput|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::ASBuild))           { result += "ASBuild|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::RayTracingShader))  { result += "RayTracingShader|"; }
	if (lib::HasAnyFlag(stage, EPipelineStage::Host))              { result += "Host|"; }

	return result;
}


using PipelineStatisticValue = std::variant<Bool, Uint64, Int64, Real64>;


struct PipelineStatistic
{
	lib::String            name;
	lib::String            description;
	PipelineStatisticValue value;
};


struct PipelineStatistics
{
	lib::DynamicArray<PipelineStatistic> statistics;
};

} // spt::rhi
