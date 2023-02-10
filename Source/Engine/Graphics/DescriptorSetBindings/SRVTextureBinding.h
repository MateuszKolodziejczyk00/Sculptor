#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType>
class SRVTextureBinding : public gfx::priv::TextureBindingBase
{
protected:

	using Super = gfx::priv::TextureBindingBase;

public:

	explicit SRVTextureBinding(const lib::HashedString& name)
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
		BuildRGDependenciesImpl(builder, rg::ERGTextureAccess::SampledTexture);
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("Texture") + priv::GetTextureDimSuffix<dimensions>()
										+ '<' + rdr::shader_translator::GetShaderTypeName<TPixelFormatType>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::None;
	}

	template<priv::CTextureInstanceOrRGTextureView TType>
	SRVTextureBinding& operator=(const TType& value)
	{
		Set(value);
		return *this;
	}

private:

	void ValidateTexture(const lib::SharedRef<rdr::TextureView>& textureView) const
	{
		const lib::SharedPtr<rdr::Texture> texture = textureView->GetTexture();
		SPT_CHECK(!!texture);
		SPT_CHECK(lib::HasAnyFlag(texture->GetRHI().GetDefinition().usage, rhi::ETextureUsage::SampledTexture));
	}
};


template<typename TPixelFormatType>
using SRVTexture1DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType>;

template<typename TPixelFormatType>
using SRVTexture2DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType>;

template<typename TPixelFormatType>
using SRVTexture3DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType>;

} // spt::gfx
