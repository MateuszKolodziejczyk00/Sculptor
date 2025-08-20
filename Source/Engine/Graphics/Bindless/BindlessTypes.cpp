#include "BindlessTypes.h"
#include "RGResources/RGResources.h"

namespace spt::gfx
{

void ValidateTextureBinding(const std::variant<lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle>& boundView, TextureDescriptorMetadata metadata)
{
	const rhi::TextureDefinition* textureDef = nullptr;

	Bool isValid       = false;
	Bool canCheckUsage = false;

	lib::HashedString resourceName;

	if (std::holds_alternative<lib::SharedPtr<rdr::TextureView>>(boundView))
	{
		const lib::SharedPtr<rdr::TextureView>& textureView = std::get<lib::SharedPtr<rdr::TextureView>>(boundView);
		if (textureView)
		{
			textureDef    = &textureView->GetTexture()->GetDefinition();
			isValid       = true;
			canCheckUsage = true;
			resourceName  = textureView->GetTexture()->GetRHI().GetName();
		}
	}
	else
	{
		const rg::RGTextureViewHandle& textureView = std::get<rg::RGTextureViewHandle>(boundView);
		if (textureView.IsValid())
		{
			isValid      = true;
			textureDef   = &textureView->GetTextureRHIDefinition();
			resourceName = textureView->GetName();
		}
	}

	if (!metadata.isOptional)
	{
		SPT_CHECK_MSG(isValid, "TextureDescriptor is not valid");
	}

	if (!isValid)
	{
		return;
	}

	SPT_CHECK(!!textureDef);

	const rhi::ETextureType textureType = rhi::GetSelectedTextureType(*textureDef);

	if (metadata.dimentions == 1)
	{
		SPT_CHECK_MSG(textureType == rhi::ETextureType::Texture1D, "TextureDescriptor ({}) is not valid for 1D texture", resourceName.ToString());
	}
	else if (metadata.dimentions == 2)
	{
		SPT_CHECK_MSG(textureType == rhi::ETextureType::Texture2D, "TextureDescriptor ({}) is not valid for 2D texture", resourceName.ToString());
	}
	else if (metadata.dimentions == 3)
	{
		SPT_CHECK_MSG(textureType == rhi::ETextureType::Texture3D, "TextureDescriptor ({}) is not valid for 3D texture", resourceName.ToString());
	}

	if (canCheckUsage)
	{
		if (metadata.isUAV)
		{
			SPT_CHECK_MSG(lib::HasAnyFlag(textureDef->usage, rhi::ETextureUsage::StorageTexture), "TextureDescriptor ({}) is not valid for UAV texture", resourceName.ToString());
		}
		else
		{
			SPT_CHECK_MSG(lib::HasAnyFlag(textureDef->usage, rhi::ETextureUsage::SampledTexture), "TextureDescriptor ({}) is not valid for SRV texture", resourceName.ToString());
		}
	}
}

} // spt::gfx