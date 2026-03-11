#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "RHICore/RHITextureTypes.h"
#include "ShaderStructs/ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rdr
{
class Texture;
class TextureView;
} // spt::rdr


namespace spt::rsc
{

enum class EShadowMappingTechnique
{
	None,
	DPCF,
	CSM,
	VSM
};


enum class EShadowMapType
{
	None,
	PointLightFace,
	DirectionalLightCascade
};


struct ShadowMapViewComponent
{
	ShadowMapViewComponent()
		: faceIdx(0)
	{ }

	lib::SharedPtr<rdr::Texture>           shadowMap;
	RenderSceneEntity                      owningLight;
	Uint32                                 faceIdx;
	EShadowMapType                         shadowMapType;
	std::optional<EShadowMappingTechnique> techniqueOverride;
};


struct ShadowMapUtils
{
public:

	static rhi::TextureDefinition CreateShadowMapDefinition(math::Vector2u resolution, EShadowMappingTechnique technique);
	
	static rhi::EFragmentFormat   GetShadowMapFormat(EShadowMappingTechnique technique);
	
	static rhi::ETextureUsage     GetShadowMapUsage(EShadowMappingTechnique technique);
	
	static Uint32                 GetShadowMapMipsNum(EShadowMappingTechnique technique);
};


BEGIN_SHADER_STRUCT(ShadowMapViewData)
	SHADER_STRUCT_FIELD(math::Matrix4f, viewProjectionMatrix)
END_SHADER_STRUCT();


struct ShadowMap
{
	lib::SharedPtr<rdr::TextureView> textureView;
	ShadowMapViewData                viewData;
};


BEGIN_SHADER_STRUCT(ShadowsSettings)
	SHADER_STRUCT_FIELD(Uint32, highQualitySMEndIdx)
	SHADER_STRUCT_FIELD(Uint32, mediumQualitySMEndIdx)
	SHADER_STRUCT_FIELD(math::Vector2f, highQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, mediumQualitySMPixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, lowQualitySMPixelSize)
	SHADER_STRUCT_FIELD(Real32, shadowViewsNearPlane)
	SHADER_STRUCT_FIELD(Uint32, shadowMappingTechnique)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ShadowMapTexture)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>, texture)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ShadowMapsData)
	SHADER_STRUCT_FIELD(ShadowsSettings,                        settings)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<ShadowMapViewData>, shadowMapViews)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<ShadowMapTexture>,  shadowMaps)
END_SHADER_STRUCT();

} // spt::rsc
