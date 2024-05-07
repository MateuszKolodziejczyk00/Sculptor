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
