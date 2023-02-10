#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

struct DepthPrepassData
{
	rg::RGTextureViewHandle depth;
};


struct ShadingData
{
	rg::RGTextureViewHandle radiance;
	rg::RGTextureViewHandle normals;
};


struct HDRResolvePassData
{
	rg::RGTextureViewHandle tonemappedTexture;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, SceneRendererDebugSettings)
	SHADER_STRUCT_FIELD(Bool, showDebugMeshlets)
	SHADER_STRUCT_FIELD(math::Vector3f, padding)
END_SHADER_STRUCT();

} // spt::rsc