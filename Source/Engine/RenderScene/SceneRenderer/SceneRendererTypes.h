#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

struct DepthPrepassResult
{
	rg::RGTextureViewHandle depth;
};


struct ForwardOpaquePassResult
{
	rg::RGTextureViewHandle radiance;
	rg::RGTextureViewHandle normals;
	rg::RGTextureViewHandle tonamappedTexture;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, SceneRendererDebugSettings)
	SHADER_STRUCT_FIELD(Bool, showDebugMeshlets)
	SHADER_STRUCT_FIELD(math::Vector3f, padding)
END_SHADER_STRUCT();

} // spt::rsc