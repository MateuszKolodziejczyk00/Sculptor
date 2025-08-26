#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

BEGIN_ALIGNED_SHADER_STRUCT(16, DirectionalLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, illuminance) // Lux
	SHADER_STRUCT_FIELD(Real32,         sunDiskEC)
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
	SHADER_STRUCT_FIELD(Real32,         sunDiskMinCosAngle)
	SHADER_STRUCT_FIELD(math::Vector3f, outerSpaceIlluminance) // Lux
	SHADER_STRUCT_FIELD(Uint32,         shadowMaskIdx)
	SHADER_STRUCT_FIELD(Uint32,         firstShadowCascadeIdx)
	SHADER_STRUCT_FIELD(Uint32,         shadowCascadesNum)
END_SHADER_STRUCT();


BEGIN_ALIGNED_SHADER_STRUCT(16, PointLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, luminousPower) // Lumens
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32, radius)
	SHADER_STRUCT_FIELD(Uint32, entityID)
	SHADER_STRUCT_FIELD(Uint32, shadowMapFirstFaceIdx)
END_SHADER_STRUCT();


// Illuminance in the atmosphere
struct DirectionalLightIlluminance
{
	// Both values in lux
	math::Vector3f outerSpaceIlluminance;
	math::Vector3f illuminance; // based on atmosphere transmittance in the direction of the light
};
SPT_REGISTER_COMPONENT_TYPE(DirectionalLightIlluminance, RenderSceneRegistry);


struct DirectionalLightData
{
	DirectionalLightData()
		: color(math::Vector3f::Ones())
		, zenithIlluminance(1.f)
		, direction(-math::Vector3f::UnitZ())
		, lightConeAngle(0.f)
		, sunDiskAngleMultiplier(1.f)
		, sunDiskEC(0.f)
	{ }

	math::Vector3f	color;

	// Illuminance in lux
	Real32			zenithIlluminance;

	math::Vector3f	direction;
	Real32			lightConeAngle;
	
	Real32			sunDiskAngleMultiplier;
	
	Real32			sunDiskEC;
};
SPT_REGISTER_COMPONENT_TYPE(DirectionalLightData, RenderSceneRegistry);


struct PointLightData
{
	PointLightData()
		: color(math::Vector3f::Ones())
		, luminousPower(1.f)
		, location(math::Vector3f::Zero())
		, radius(1.f)
	{ }

	math::Vector3f	color;

	// Luminous power in lumens
	Real32			luminousPower;

	math::Vector3f	location;
	Real32			radius;
};
SPT_REGISTER_COMPONENT_TYPE(PointLightData, RenderSceneRegistry);


struct GPUDataBuilder
{
public:

	static DirectionalLightGPUData CreateDirectionalLightGPUData(const DirectionalLightData& lightData, const DirectionalLightIlluminance& illuminance)
	{
		DirectionalLightGPUData gpuData;
		gpuData.illuminance           = illuminance.illuminance;
		gpuData.outerSpaceIlluminance = illuminance.outerSpaceIlluminance;
		gpuData.direction             = lightData.direction;
		gpuData.sunDiskEC             = lightData.sunDiskEC;
		gpuData.sunDiskMinCosAngle    = lightData.lightConeAngle * lightData.sunDiskAngleMultiplier;
		return gpuData;
	}

	static PointLightGPUData CreatePointLightGPUData(const PointLightData& lightData)
	{
		PointLightGPUData gpuData;
		gpuData.color         = lightData.color;
		gpuData.luminousPower = lightData.luminousPower;
		gpuData.location      = lightData.location;
		gpuData.radius        = lightData.radius;
		return gpuData;
	}
};

} // spt::rsc