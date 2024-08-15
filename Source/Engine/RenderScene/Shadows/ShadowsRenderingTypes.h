#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "RHICore/RHITextureTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


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

} // spt::rsc