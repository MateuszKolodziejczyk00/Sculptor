#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "SceneRenderSystems/Atmosphere/AtmosphereTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc::clouds
{

BEGIN_SHADER_STRUCT(CloudscapeConstants)
	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereCenterZ)

	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereInnerRadius)
	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereOuterRadius)

	SHADER_STRUCT_FIELD(Real32, baseShapeNoiseScale)
	SHADER_STRUCT_FIELD(Real32, weatherMapScale)

	SHADER_STRUCT_FIELD(Real32, detailShapeNoiseStrength0)
	SHADER_STRUCT_FIELD(Real32, detailShapeNoiseScale0)
	SHADER_STRUCT_FIELD(Real32, detailShapeNoiseStrength1)
	SHADER_STRUCT_FIELD(Real32, detailShapeNoiseScale1)

	SHADER_STRUCT_FIELD(Real32, curlNoiseScale)
	SHADER_STRUCT_FIELD(Real32, curlMaxoffset)

	SHADER_STRUCT_FIELD(Real32, cloudscapeRange)
	SHADER_STRUCT_FIELD(Real32, cloudscapeInnerHeight)
	SHADER_STRUCT_FIELD(Real32, cloudscapeOuterHeight)

	SHADER_STRUCT_FIELD(math::Vector3f, cloudsAtmosphereCenter)

	SHADER_STRUCT_FIELD(Real32, globalDensity)
	SHADER_STRUCT_FIELD(Real32, globalCoverageOffset)
	SHADER_STRUCT_FIELD(Real32, globalCloudsHeightOffset)
	SHADER_STRUCT_FIELD(Real32, globalCoverageMultiplier)
	SHADER_STRUCT_FIELD(Real32, globalCloudsHeightMultiplier)

	SHADER_STRUCT_FIELD(math::Vector2f, probesSpacing)
	SHADER_STRUCT_FIELD(math::Vector2f, rcpProbesSpacing)
	SHADER_STRUCT_FIELD(math::Vector2f, probesOrigin)
	SHADER_STRUCT_FIELD(math::Vector2u, probesNum)
	SHADER_STRUCT_FIELD(math::Vector2u, pixelsPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, uvPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, uvPerProbeNoBorders)
	SHADER_STRUCT_FIELD(math::Vector2f, uvBorder)
	SHADER_STRUCT_FIELD(Real32,         probesHeight)

	SHADER_STRUCT_FIELD(math::Vector2u, highResProbeRes)
	SHADER_STRUCT_FIELD(math::Vector2f, highResProbeRcpRes)

	SHADER_STRUCT_FIELD(math::Vector2f, shadowsCacheOrigin)
	SHADER_STRUCT_FIELD(math::Vector2f, shadowsCacheSize)
	SHADER_STRUCT_FIELD(math::Vector2f, shadowsCacheRcpSize)
	SHADER_STRUCT_FIELD(math::Vector3f, shadowsCacheVoxelSize)

	SHADER_STRUCT_FIELD(DirectionalLightGPUData, mainDirectionalLight)

	SHADER_STRUCT_FIELD(Real32, time)

	SHADER_STRUCT_FIELD(rdr::GPUPtr<AtmosphereParams>, atmosphereConstants)

	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, transmittanceLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, baseShapeNoise)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector4f>, detailShapeNoise)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<math::Vector3f>, curlNoise)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, weatherMap)
	SHADER_STRUCT_FIELD(gfx::SRVTexture3D<Real32>,         shadowsCache)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         densityLUT)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector2f>, cirrusCloudsMask)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(CloudscapeProbesParams)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, cloudscapeProbes)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, cloudscapeHighResProbe)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<CloudscapeConstants>,  cloudscapeConstants)
END_SHADER_STRUCT();


struct CloudscapeContext
{
	const AtmosphereContext&        atmosphere;
	const CloudscapeConstants&      cloudscapeConstants;

	rdr::GPUPtr<CloudscapeConstants> gpuCloudscapeConstants;

	Bool resetAccumulation = false;

	Uint32 frameIdx = 0u;
};


struct CloudsTransmittanceMap
{
	rg::RGTextureViewHandle cloudsTransmittanceTexture;
	math::Matrix4f          viewProjectionMatrix;
};

} // spt::rsc::clouds
