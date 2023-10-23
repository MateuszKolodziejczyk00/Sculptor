#pragma once

#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferRefBinding.h"
#include "LightTypes.h"
#include "Atmosphere/AtmosphereTypes.h"
#include "RHICore/RHISamplerTypes.h"
#include "Shadows/ShadowsRenderingTypes.h"


namespace spt::rsc
{

class ShadowMapsDS;

BEGIN_SHADER_STRUCT(LightsRenderingData)
	SHADER_STRUCT_FIELD(Uint32,			localLightsNum)
	SHADER_STRUCT_FIELD(Uint32,			localLights32Num)
	SHADER_STRUCT_FIELD(Uint32,			directionalLightsNum)
	SHADER_STRUCT_FIELD(Real32,			zClusterLength)
	SHADER_STRUCT_FIELD(math::Vector2u,	tilesNum)
	SHADER_STRUCT_FIELD(math::Vector2f,	tileSize)
	SHADER_STRUCT_FIELD(Uint32,			zClustersNum)
	SHADER_STRUCT_FIELD(Real32,			ambientLightIntensity)
END_SHADER_STRUCT();


DS_BEGIN(ViewShadingInputDS, rg::RGDescriptorSetState<ViewShadingInputDS>)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<LightsRenderingData>),				u_lightsData)
	DS_BINDING(BINDING_TYPE(gfx::OptionalRWStructuredBufferBinding<math::Vector2u>),				u_clustersRanges) // min,max light idx for each z cluster
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<PointLightGPUData>),				u_localLights)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<Uint32>),							u_tilesLightsMask)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<DirectionalLightGPUData>),			u_directionalLights)
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<4, true>),								u_shadowMasks)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_nearestSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearClampToEdge>),	u_linearSampler)
	DS_BINDING(BINDING_TYPE(gfx::OptionalSRVTexture2DBinding<Real32>),								u_ambientOcclusionTexture)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<math::Vector3f>),								u_transmittanceLUT)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferRefBinding<AtmosphereParams>),						u_atmosphereParams)
	
	DS_BINDING(BINDING_TYPE(gfx::ArrayOfSRVTextures2DBinding<8, true>),								u_shadowMapCascades)
	DS_BINDING(BINDING_TYPE(gfx::OptionalStructuredBufferBinding<ShadowMapViewData>),				u_shadowMapCascadeViews)
DS_END();


struct ViewDirectionalShadowMasksData
{
	lib::HashMap<RenderSceneEntity, rg::RGTextureViewHandle> shadowMasks;
};


struct ViewSpecShadingParameters
{
	lib::MTHandle<ViewShadingInputDS>	shadingInputDS;
	lib::MTHandle<ShadowMapsDS>		shadowMapsDS;
};

} // spt::rsc