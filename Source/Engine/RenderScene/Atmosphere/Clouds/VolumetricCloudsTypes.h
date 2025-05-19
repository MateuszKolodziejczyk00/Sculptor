#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "Atmosphere/AtmosphereTypes.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc::clouds
{

BEGIN_SHADER_STRUCT(CloudscapeConstants)
	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereCenterZ)

	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereInnerRadius)
	SHADER_STRUCT_FIELD(Real32, cloudsAtmosphereOuterRadius)

	SHADER_STRUCT_FIELD(Real32, baseShapeNoiseScale)
	SHADER_STRUCT_FIELD(Real32, detailShapeNoiseScale)
	SHADER_STRUCT_FIELD(Real32, weatherMapScale)

	SHADER_STRUCT_FIELD(Real32, cloudscapeRange)
	SHADER_STRUCT_FIELD(Real32, cloudscapeInnerHeight)
	SHADER_STRUCT_FIELD(Real32, cloudscapeOuterHeight)

	SHADER_STRUCT_FIELD(math::Vector3f, cloudsAtmosphereCenter)

	SHADER_STRUCT_FIELD(Real32, globalDensity)
	SHADER_STRUCT_FIELD(Real32, globalCoverage)

	SHADER_STRUCT_FIELD(Real32, time)
END_SHADER_STRUCT();


DS_BEGIN(CloudscapeDS, rg::RGDescriptorSetState<CloudscapeDS>)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<CloudscapeConstants>),                    u_cloudscapeConstants)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),                    u_atmosphereConstants)
	DS_BINDING(BINDING_TYPE(gfx::StructuredBufferBinding<DirectionalLightGPUData>),              u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>), u_linearClampSampler)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<Real32>),                                   u_baseShapeNoise)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture3DBinding<math::Vector4f>),                           u_detailShapeNoise)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_curlNoise)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),                           u_weatherMap)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearRepeat>),      u_linearRepeatSampler)
DS_END();


struct CloudscapeContext
{
	const AtmosphereContext&    atmosphere;
	const CloudscapeConstants&  cloudscapeConstants;
	lib::MTHandle<CloudscapeDS> cloudscapeDS;
};


struct CloudsTransmittanceMap
{
	rg::RGTextureViewHandle cloudsTransmittanceTexture;
	math::Matrix4f          viewProjectionMatrix;
};

} // spt::rsc::clouds
