#include "ShadowsRenderingTypes.h"


namespace spt::rsc
{

rhi::TextureDefinition ShadowMapUtils::CreateShadowMapDefinition(math::Vector2u resolution, EShadowMappingTechnique technique)
{
	rhi::TextureDefinition definition;
	definition.resolution  = resolution;
	definition.format      = GetShadowMapFormat(technique);
	definition.usage       = GetShadowMapUsage(technique);
	definition.mipLevels   = GetShadowMapMipsNum(technique);

	return definition;
}

rhi::EFragmentFormat ShadowMapUtils::GetShadowMapFormat(EShadowMappingTechnique technique)
{
	switch (technique)
	{
	case EShadowMappingTechnique::DPCF:
		return rhi::EFragmentFormat::D16_UN_Float;

	case EShadowMappingTechnique::MSM:
		return rhi::EFragmentFormat::RGBA16_UN_Float;
	
	case EShadowMappingTechnique::VSM:
		return rhi::EFragmentFormat::RG16_UN_Float;

	default:
		SPT_CHECK_NO_ENTRY();
		return rhi::EFragmentFormat::None;
	}
}

rhi::ETextureUsage ShadowMapUtils::GetShadowMapUsage(EShadowMappingTechnique technique)
{
	rhi::ETextureUsage usage = rhi::ETextureUsage::None;

	switch (technique)
	{
	case EShadowMappingTechnique::DPCF:
		usage = lib::Flags(rhi::ETextureUsage::DepthSetncilRT, rhi::ETextureUsage::SampledTexture);
		break;

	case EShadowMappingTechnique::MSM:
		usage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
		break;
	
	case EShadowMappingTechnique::VSM:
		usage = lib::Flags(rhi::ETextureUsage::StorageTexture, rhi::ETextureUsage::SampledTexture);
		break;

	default:
		SPT_CHECK_NO_ENTRY();
	}

#if SPT_DEBUG || SPT_DEVELOPMENT
		lib::AddFlag(usage, rhi::ETextureUsage::TransferSource);
#endif

	return usage;
}

Uint32 ShadowMapUtils::GetShadowMapMipsNum(EShadowMappingTechnique technique)
{
	switch (technique)
	{
	case EShadowMappingTechnique::DPCF:
		return 1u;

	case EShadowMappingTechnique::MSM:
		return 5u;
	
	case EShadowMappingTechnique::VSM:
		return 5u;

	default:
		SPT_CHECK_NO_ENTRY();
		return 0;
	}
}

} // spt::rsc
