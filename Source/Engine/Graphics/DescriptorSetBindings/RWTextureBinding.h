#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType>
class RWTextureBinding : public gfx::priv::TextureBindingBase
{
protected:

	using Super = gfx::priv::TextureBindingBase;

public:

	explicit RWTextureBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		const lib::SharedRef<rdr::TextureView> textureView = GetTextureToBind();
#if DO_CHECKS
		ValidateTexture(textureView);
#endif // DO_CHECKS

		context.UpdateTexture(GetName(), textureView);
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		BuildRGDependenciesImpl(builder, rg::ERGTextureAccess::StorageWriteTexture);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("RWTexture") + priv::GetTextureDimSuffix<dimensions>()
										+ '<' + rdr::shader_translator::GetShaderTypeName<TPixelFormatType>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::Storage;
	}

	template<priv::CTextureInstanceOrRGTextureView TType>
	RWTextureBinding& operator=(const TType& value)
	{
		Set(value);
		return *this;
	}

private:

	void ValidateTexture(const lib::SharedRef<rdr::TextureView>& textureView) const
	{
		const lib::SharedPtr<rdr::Texture> texture = textureView->GetTexture();
		SPT_CHECK(!!texture);
		SPT_CHECK(lib::HasAnyFlag(texture->GetRHI().GetDefinition().usage, rhi::ETextureUsage::StorageTexture));
	}
};


template<typename TPixelFormatType>
using RWTexture1DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType>;

template<typename TPixelFormatType>
using RWTexture2DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType>;

template<typename TPixelFormatType>
using RWTexture3DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType>;

} // spt::gfx
