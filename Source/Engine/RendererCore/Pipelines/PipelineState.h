#pragma once

#include "SculptorCoreTypes.h"
#include "Shaders/ShaderTypes.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"


namespace spt::rdr
{

struct GraphicsPipelineState
{
	GraphicsPipelineState() = default;

	rhi::EPrimitiveTopology					primitiveTopology;
	rhi::PipelineRasterizationDefinition	rasterizationDefinition;
	ShaderID								shader;
};


using PipelineStateID = SizeType;

} // spt::rdr