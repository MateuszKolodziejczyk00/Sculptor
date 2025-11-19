#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RenderSceneRegistry.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(DirectionalLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, illuminance) // Lux
	SHADER_STRUCT_FIELD(Real32,         sunDiskEC)
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
	SHADER_STRUCT_FIELD(Real32,         sunDiskMinCosAngle)
	SHADER_STRUCT_FIELD(math::Vector3f, outerSpaceIlluminance) // Lux
	SHADER_STRUCT_FIELD(Uint32,         shadowMaskIdx)
	SHADER_STRUCT_FIELD(Uint32,         firstShadowCascadeIdx)
	SHADER_STRUCT_FIELD(Uint32,         shadowCascadesNum)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(PointLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, luminousPower) // Lumens
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32, radius)
	SHADER_STRUCT_FIELD(Uint32, entityID)
	SHADER_STRUCT_FIELD(Uint32, shadowMapFirstFaceIdx)
END_SHADER_STRUCT();


enum class ELocalLightType : Uint32
{
	PointLight,
	SpotLight,
	NUM
};


BEGIN_SHADER_STRUCT(LocalLightGPUData)
	SHADER_STRUCT_FIELD(Uint32,         type)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32,         luminousPower) // Lumens
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32,         range)
	SHADER_STRUCT_FIELD(Uint32,         entityID)
	SHADER_STRUCT_FIELD(Uint32,         shadowMapFirstFaceIdx)
	// Spot only begin
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
	SHADER_STRUCT_FIELD(Real32,         halfAngleCos)
	SHADER_STRUCT_FIELD(Real32,         halfAngleTan)
	SHADER_STRUCT_FIELD(Real32,         boundingSphereRadius)
	SHADER_STRUCT_FIELD(Real32,         lightAngleScale)
	SHADER_STRUCT_FIELD(Real32,         lightAngleOffset)
	// Spot only end
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
	math::Vector3f color = math::Vector3f::Ones();

	// Illuminance in lux
	Real32 zenithIlluminance = 1.f;

	math::Vector3f direction      = -math::Vector3f::UnitZ();
	Real32         lightConeAngle = 0.f;

	// Multiplier applied only for visible sun disk
	Real32 sunDiskAngleMultiplier = 1.f;
	Real32 sunDiskEC              = 0.f;
};
SPT_REGISTER_COMPONENT_TYPE(DirectionalLightData, RenderSceneRegistry);


struct PointLightData
{
	math::Vector3f color = math::Vector3f::Ones();

	// Luminous power in lumens
	Real32 luminousPower = 1.f;

	math::Vector3f location = math::Vector3f::Zero();
	Real32         radius   = 1.f;
};
SPT_REGISTER_COMPONENT_TYPE(PointLightData, RenderSceneRegistry);


struct SpotLightData
{
	math::Vector3f	color = math::Vector3f::Ones();

	// Luminous power in lumens
	Real32 luminousPower = 1.f;

	math::Vector3f location       = math::Vector3f::Zero();
	math::Vector3f direction      = math::Vector3f::UnitX();
	Real32         range          = 1.f;
	Real32         innerHalfAngle = 22.5f;
	Real32         outerHalfAngle = 45.f;
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

	static LocalLightGPUData CreatePointLightGPUData(const PointLightData& lightData)
	{
		LocalLightGPUData gpuData;
		gpuData.type          = static_cast<Uint32>(ELocalLightType::PointLight);
		gpuData.color         = lightData.color;
		gpuData.luminousPower = lightData.luminousPower;
		gpuData.location      = lightData.location;
		gpuData.range         = lightData.radius;
		return gpuData;
	}

	static LocalLightGPUData CreateSpotLightGPUData(const SpotLightData& lightData)
	{
		const Real32 innerAngleRadians = math::Utils::DegreesToRadians(lightData.innerHalfAngle);
		const Real32 outerAngleRadians = math::Utils::DegreesToRadians(lightData.outerHalfAngle);

		const Real32 innerAngleCos = std::cos(innerAngleRadians);
		const Real32 outerAngleCos = std::cos(outerAngleRadians);

		Real32 boundingSphereRadius;
		if (lightData.outerHalfAngle < 45.f)
		{
			boundingSphereRadius = lightData.range  / (2.f * outerAngleCos * outerAngleCos);
		}
		else
		{
			boundingSphereRadius = lightData.range * std::tan(outerAngleRadians);
		}

		LocalLightGPUData gpuData;
		gpuData.type                 = static_cast<Uint32>(ELocalLightType::SpotLight);
		gpuData.color                = lightData.color;
		gpuData.luminousPower        = lightData.luminousPower;
		gpuData.location             = lightData.location;
		gpuData.range                = lightData.range;
		gpuData.direction            = lightData.direction;
		gpuData.halfAngleCos         = outerAngleCos;
		gpuData.halfAngleTan         = std::tan(outerAngleRadians);
		gpuData.boundingSphereRadius = boundingSphereRadius;
		gpuData.lightAngleScale      = 1.f / std::max(0.001f, (innerAngleCos - outerAngleCos));
		gpuData.lightAngleOffset     = -outerAngleCos * gpuData.lightAngleScale;

		return gpuData;
	}
};

} // spt::rsc