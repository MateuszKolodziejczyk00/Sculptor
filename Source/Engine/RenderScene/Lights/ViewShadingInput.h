#pragma once

#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "DescriptorSetBindings/RWBufferBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ArrayOfSRVTexturesBinding.h"
#include "LightTypes.h"
#include "RHICore/RHISamplerTypes.h"


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
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::NearestClampToEdge>),	u_shadowMaskSampler)
DS_END();


struct ViewSpecShadingParameters
{
	lib::SharedPtr<ViewShadingInputDS>	shadingInputDS;
	lib::SharedPtr<ShadowMapsDS>		shadowMapsDS;
};

} // spt::rsc