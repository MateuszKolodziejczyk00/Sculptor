#pragma once

#include "RenderInstancesList.h"
#include "ShaderStructs/ShaderStructsMacros.h"

namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(, 16, BasePassRenderData)
	SHADER_STRUCT_FIELD(Uint32, transformIdx)
	SHADER_STRUCT_FIELD(Uint32, firstPrimitiveIdx)
	SHADER_STRUCT_FIELD(Uint32, primitivesNum)
END_SHADER_STRUCT();


struct BasePassRenderDataHandle
{
	rhi::RHISuballocation basePassInstanceData;
};

using BasePassRenderInstancesList = RenderInstancesList<BasePassRenderData>;

} // spt::rsc
