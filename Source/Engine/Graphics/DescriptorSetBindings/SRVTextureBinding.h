#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType, Bool isOptional>
class SRVTextureBinding : public gfx::priv::TextureBindingBase
{
protected:

	using Super = gfx::priv::TextureBindingBase;

public:

	explicit SRVTextureBinding(const lib::HashedString& name)
		: Super(name)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		const Bool isBound = IsValid();
		if constexpr (!isOptional)
		{
			SPT_CHECK_MSG(isBound, "Unbound binding \"{}\" in Descriptor set \"{}\"", GetName().GetData(), GetDescriptorSetName().GetData());
		}

		if (isBound)
		{
			const lib::SharedRef<rdr::TextureView> textureView = GetTextureToBind();
#if DO_CHECKS
			ValidateTexture(textureView);
#endif // DO_CHECKS

			textureView->GetRHI().CopySRVDescriptor(indexer[GetBaseBindingIdx()][0]);
		}
	}

	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		const Bool isBound = IsValid();
		if constexpr (!isOptional)
		{
			SPT_CHECK_MSG(isBound, "Unbound binding \"{}\" in Descriptor set \"{}\"", GetName().GetData(), GetDescriptorSetName().GetData());
		}

		if (isBound)
		{
			BuildRGDependenciesImpl(builder, rg::ERGTextureAccess::SampledTexture);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("Texture") + priv::GetTextureDimSuffix<dimensions>()
										+ '<' + rdr::shader_translator::StructTranslator<TPixelFormatType>::GetHLSLStructName() + "> " + name, bindingIdx);
	}
	
	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::None;

		if constexpr (isOptional)
		{
			lib::AddFlag(flags, smd::EBindingFlags::PartiallyBound);
		}

		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::SampledTexture, flags) };
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
using SRVTexture1DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, false>;

template<typename TPixelFormatType>
using SRVTexture2DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, false>;

template<typename TPixelFormatType>
using SRVTexture3DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, false>;


template<typename TPixelFormatType>
using OptionalSRVTexture1DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, true>;

template<typename TPixelFormatType>
using OptionalSRVTexture2DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, true>;

template<typename TPixelFormatType>
using OptionalSRVTexture3DBinding = SRVTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, true>;

} // spt::gfx
