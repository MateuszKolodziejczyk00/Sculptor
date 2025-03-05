#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(DirectionalLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, illuminance) // Lux
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
	SHADER_STRUCT_FIELD(Uint32, shadowMaskIdx)
	SHADER_STRUCT_FIELD(Uint32, firstShadowCascadeIdx)
	SHADER_STRUCT_FIELD(Uint32, shadowCascadesNum)
	SHADER_STRUCT_FIELD(Real32, sunDiskMinCosAngle)
	SHADER_STRUCT_FIELD(Real32, sunDiskEC)
END_SHADER_STRUCT();


BEGIN_ALIGNED_SHADER_STRUCT(16, PointLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, luminousPower) // Lumens
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32, radius)
	SHADER_STRUCT_FIELD(Uint32, entityID)
	SHADER_STRUCT_FIELD(Uint32, shadowMapFirstFaceIdx)
END_SHADER_STRUCT();


struct DirectionalLightData
{
	DirectionalLightData()
		: color(math::Vector3f::Ones())
		, illuminance(1.f)
		, direction(-math::Vector3f::UnitZ())
		, lightConeAngle(0.f)
		, sunDiskAngleMultiplier(1.f)
		, sunDiskEC(0.f)
	{ }

	DirectionalLightGPUData GenerateGPUData() const
	{
		DirectionalLightGPUData gpuData;
		gpuData.color              = color;
		gpuData.illuminance        = illuminance;
		gpuData.direction          = direction;
		gpuData.sunDiskEC          = sunDiskEC;
		gpuData.sunDiskMinCosAngle = lightConeAngle * sunDiskAngleMultiplier;

		return gpuData;
	}

	math::Vector3f	color;

	// Illuminance in lux
	Real32			illuminance;

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

	PointLightGPUData GenerateGPUData() const
	{
		PointLightGPUData gpuData;
		gpuData.color			= color;
		gpuData.luminousPower	= luminousPower;
		gpuData.location		= location;
		gpuData.radius			= radius;

		return gpuData;
	}

	math::Vector3f	color;

	// Luminous power in lumens
	Real32			luminousPower;

	math::Vector3f	location;
	Real32			radius;
};
SPT_REGISTER_COMPONENT_TYPE(PointLightData, RenderSceneRegistry);

} // spt::rsc