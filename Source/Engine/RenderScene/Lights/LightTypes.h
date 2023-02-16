#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(DirectionalLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, intensity)
	SHADER_STRUCT_FIELD(math::Vector3f, direction)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(PointLightGPUData)
	SHADER_STRUCT_FIELD(math::Vector3f, color)
	SHADER_STRUCT_FIELD(Real32, intensity)
	SHADER_STRUCT_FIELD(math::Vector3f, location)
	SHADER_STRUCT_FIELD(Real32, radius)
	SHADER_STRUCT_FIELD(Uint32, entityID)
	SHADER_STRUCT_FIELD(Uint32, shadowMapFirstFaceIdx)
END_SHADER_STRUCT();


struct DirectionalLightData
{
	DirectionalLightData()
		: color(math::Vector3f::Ones())
		, intensity(1.f)
		, direction(-math::Vector3f::UnitZ())
	{ }

	DirectionalLightGPUData GenerateGPUData() const
	{
		DirectionalLightGPUData gpuData;
		gpuData.color		= color;
		gpuData.intensity	= intensity;
		gpuData.direction	= direction;

		return gpuData;
	}

	math::Vector3f	color;
	Real32			intensity;
	math::Vector3f	direction;
};


struct PointLightData
{
	PointLightData()
		: color(math::Vector3f::Ones())
		, intensity(1.f)
		, location(math::Vector3f::Zero())
		, radius(1.f)
	{ }

	PointLightGPUData GenerateGPUData() const
	{
		PointLightGPUData gpuData;
		gpuData.color		= color;
		gpuData.intensity	= intensity;
		gpuData.location	= location;
		gpuData.radius		= radius;

		return gpuData;
	}

	math::Vector3f	color;
	Real32			intensity;
	math::Vector3f	location;
	Real32			radius;
};

} // spt::rsc