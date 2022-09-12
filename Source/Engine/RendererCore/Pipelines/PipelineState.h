#pragma once

#include "SculptorCoreTypes.h"
#include "Shaders/ShaderTypes.h"


namespace spt::rdr
{

struct GraphicsPipelineState
{
	GraphicsPipelineState() = default;

	rhi::EPrimitiveTopology					primitiveTopology;
	rhi::PipelineRasterizationDefinition	rasterizationDefinition;
	ShaderID								shader;
};


struct ComputePipeineState
{
	ComputePipeineState() = default;

	ShaderID shader;
};


using PipelineStateID = SizeType;

} // spt::rdr


namespace std
{

template<>
struct hash<spt::rdr::GraphicsPipelineState>
{
	constexpr size_t operator()(const spt::rdr::GraphicsPipelineState& state) const
	{
		return spt::lib::HashCombine(state.primitiveTopology,
									 state.rasterizationDefinition,
									 state.shader);
	}
};


template<>
struct hash<spt::rdr::ComputePipeineState>
{
	constexpr size_t operator()(const spt::rdr::ComputePipeineState& state) const
	{
		return spt::lib::HashCombine(state.shader);
	}
};

} // std