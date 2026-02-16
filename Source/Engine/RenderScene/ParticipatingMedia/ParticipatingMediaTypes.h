#pragma once

#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructs.h"


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

	rg::RGTextureViewHandle localLightsInScattering;
	rg::RGTextureViewHandle historyLocalLightsInScattering;

	math::Vector3u volumetricFogResolution;

	Real32 nearPlane;
	Real32 farPlane;
};


BEGIN_SHADER_STRUCT(HeightFogParams)
	SHADER_STRUCT_FIELD(math::Vector3f, albedo)
	SHADER_STRUCT_FIELD(Real32,         density)
	SHADER_STRUCT_FIELD(Real32,         extinction)
	SHADER_STRUCT_FIELD(Real32,         heightFalloff)
	// This paramter is a bit hacky. generally function for computing height fog transmittance assumes no scattering (only absorbtion).
	// This paramter helps to recover some of the radiance that would be transmitted due to forward scattering
	SHADER_STRUCT_FIELD(Real32,         absorptionPercentage)
	SHADER_STRUCT_FIELD(Real32,         padding1)
END_SHADER_STRUCT();

} // spt::rsc
