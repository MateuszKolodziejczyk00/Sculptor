#pragma once

#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

struct VolumetricFogParams 
{
	VolumetricFogParams()
		: volumetricFogResolution{}
		, nearPlane(0.f)
		, farPlane(0.f)
	{ }

	rg::RGTextureViewHandle participatingMediaTextureView;
	rg::RGTextureViewHandle indirectInScatteringTextureView;
	rg::RGTextureViewHandle inScatteringTextureView;
	rg::RGTextureViewHandle integratedInScatteringTextureView;

	rg::RGTextureViewHandle directionalLightShadowTerm;
	rg::RGTextureViewHandle historyDirectionalLightShadowTerm;

	math::Vector3u volumetricFogResolution;

	Real32 nearPlane;
	Real32 farPlane;
};


BEGIN_ALIGNED_SHADER_STRUCT(16, HeightFogParams)
	SHADER_STRUCT_FIELD(math::Vector3f, albedo)
	SHADER_STRUCT_FIELD(Real32,         density)
	SHADER_STRUCT_FIELD(Real32,         extinction)
	SHADER_STRUCT_FIELD(Real32,         heightFalloff)
	SHADER_STRUCT_FIELD(Real32,         padding0)
	SHADER_STRUCT_FIELD(Real32,         padding1)
END_SHADER_STRUCT();

} // spt::rsc
