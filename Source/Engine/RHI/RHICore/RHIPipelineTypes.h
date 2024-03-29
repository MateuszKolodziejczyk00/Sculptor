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
	FragmentShader					= BIT64(3),
	EarlyFragmentTest				= BIT64(4),
	LateFragmentTest				= BIT64(5),
	ColorRTOutput					= BIT64(6),
	ComputeShader					= BIT64(7),
	Transfer						= BIT64(8),
	BottomOfPipe					= BIT64(9),
	Copy							= BIT64(10),
	Blit							= BIT64(11),
	Clear							= BIT64(12),
	IndexInput						= BIT64(13),

	ASBuild							= BIT64(14),

	RayTracingShader				= BIT64(15),

	Host							= BIT64(15),

	ALL_GRAPHICS_SHADERS			= VertexShader | FragmentShader,

	ALL_GRAPHICS					= BIT64(50),
	ALL_TRANSFER					= BIT64(51),
	ALL_COMMANDS					= BIT64(52)
};

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
