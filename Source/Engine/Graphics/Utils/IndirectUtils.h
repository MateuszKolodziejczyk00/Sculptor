#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::gfx
{

BEGIN_SHADER_STRUCT(IndirectDrawCommand)
	SHADER_STRUCT_FIELD(Uint32, vertexCount)
	SHADER_STRUCT_FIELD(Uint32, instanceCount)
	SHADER_STRUCT_FIELD(Uint32, firstVertex)
	SHADER_STRUCT_FIELD(Uint32, firstInstance)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(IndirectDispatchCommand)
	SHADER_STRUCT_FIELD(Uint32, threadGroupsX)
	SHADER_STRUCT_FIELD(Uint32, threadGroupsY)
	SHADER_STRUCT_FIELD(Uint32, threadGroupsZ)
	SHADER_STRUCT_FIELD(Uint32, padding)
END_SHADER_STRUCT();


namespace indirect_utils
{

rg::RGBufferViewHandle CreateCounterBuffer(rg::RenderGraphBuilder& graphBuilder, Uint32 initialValue);

rg::RGBufferViewHandle CreateIndirectInstancedDrawCommand(rg::RenderGraphBuilder& graphBuilder, rg::RGBufferViewHandle instancesCount, Uint32 verticesPerInstance, Uint32 firstVertex = 0, Uint32 firstInstance = 0);

rg::RGBufferViewHandle CreateIndirectDispatchMeshCommand(rg::RenderGraphBuilder& graphBuilder, rg::RGBufferViewHandle instancesNum, Uint32 instancesPerGroup = 1u);

} // indirect_utils

} // spt::gfx
