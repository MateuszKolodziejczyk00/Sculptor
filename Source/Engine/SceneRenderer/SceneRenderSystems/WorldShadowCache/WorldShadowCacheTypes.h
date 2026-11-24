#pragma once

#include "RenderSceneMacros.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc::wsc
{

namespace constants
{
static constexpr Uint32 cascadeCount = 2u;
static constexpr Uint32 shadowMapRes = 2048u;

static constexpr lib::StaticArray<Uint32, cascadeCount> cascadeTilesPerAxis = { 2u, 4u };
static constexpr Uint32 maxTilesPerAxis = 4u;
} // constants


BEGIN_SHADER_STRUCT(WSCCascadeData)
	SHADER_STRUCT_FIELD(math::Matrix4f,            viewProj)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, shadowMap)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(WSCLightData)
	SHADER_STRUCT_FIELD(SPT_SINGLE_ARG(lib::StaticArray<WSCCascadeData, constants::cascadeCount>), cascades)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(WSCData)
	SHADER_STRUCT_FIELD(WSCLightData, dirLightsData)
END_SHADER_STRUCT();


} // spt::rsc::wsc
