#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RGResourceHandles.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"


namespace spt::rsc
{

struct SceneRendererStatics
{
	static constexpr rhi::EFragmentFormat hdrFormat = rhi::EFragmentFormat::RGBA16_S_Float;
};


BEGIN_SHADER_STRUCT(DepthCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
END_SHADER_STRUCT();


DS_BEGIN(DepthCullingDS, rg::RGDescriptorSetState<DepthCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),										u_hiZTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>),	u_hiZSampler)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableConstantBufferBinding<DepthCullingParams>),				u_depthCullingParams)
DS_END();


struct DepthPrepassData
{
	rg::RGTextureViewHandle depth;
	rg::RGTextureViewHandle depthHalfRes;

	rg::RGTextureViewHandle hiZ;
	
	rg::RGTextureViewHandle prevFrameDepth;
	rg::RGTextureViewHandle prevFrameDepthHalfRes;

	lib::SharedPtr<DepthCullingDS> depthCullingDS;

};


struct MotionData
{
	rg::RGTextureViewHandle motion;
	rg::RGTextureViewHandle motionHalfRes;
};


struct ShadingInputData
{
	// Normals created from the depth buffer
	rg::RGTextureViewHandle geometryNormals;

	rg::RGTextureViewHandle geometryNormalsHalfRes;

	rg::RGTextureViewHandle ambientOcclusion;
};


struct ShadingData
{
	rg::RGTextureViewHandle luminanceTexture;
	rg::RGTextureViewHandle normalsTexture;
	
#if RENDERER_DEBUG
	rg::RGTextureViewHandle debugTexture;
#endif // RENDERER_DEBUG
};


struct AntiAliasingData
{
	rg::RGTextureViewHandle outputTexture;
};


struct HDRResolvePassData
{
	rg::RGTextureViewHandle tonemappedTexture;

#if RENDERER_DEBUG
	rg::RGTextureViewHandle debug;
#endif // RENDERER_DEBUG
};

} // spt::rsc