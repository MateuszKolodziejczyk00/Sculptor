#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType, Bool isOptional>
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
		const Bool isValid = IsValid();
		SPT_CHECK(isValid || isOptional);

		if (isValid)
		{
			const lib::SharedRef<rdr::TextureView> textureView = GetTextureToBind();
#if DO_CHECKS
			ValidateTexture(textureView);
#endif // DO_CHECKS

			context.UpdateTexture(GetName(), textureView);
		}
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder) const
	{
		const Bool isValid = IsValid();
		SPT_CHECK(isValid || isOptional);

		if (isValid)
		{
			BuildRGDependenciesImpl(builder, rg::ERGTextureAccess::StorageWriteTexture);
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String formatString = lib::String("[[vk::image_format(\"") + GetTextureFormatString() + "\")]]\n";
		return BuildBindingVariableCode(formatString + "RWTexture" + priv::GetTextureDimSuffix<dimensions>()
										+ '<' + rdr::shader_translator::GetShaderTypeName<TPixelFormatType>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		smd::EBindingFlags flags = smd::EBindingFlags::Storage;

		if constexpr (isOptional)
		{
			lib::AddFlag(flags, smd::EBindingFlags::PartiallyBound);
		}

		return flags;
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

	static constexpr lib::String GetTextureFormatString()
	{
		if constexpr (std::is_same_v<TPixelFormatType, math::Vector4f>)
		{
			return "rgba32f";
		}
		else if constexpr (std::is_same_v<TPixelFormatType, math::Vector3f>)
		{
			return "r11g11b10f";
		}
		else if constexpr (std::is_same_v<TPixelFormatType, math::Vector2f>)
		{
			return "rg32f";
		}
		else if constexpr (std::is_same_v<TPixelFormatType, Real32>)
		{
			return "r32f";
		}

		SPT_CHECK_NO_ENTRY_MSG("Unspecified format for pixel type");
		return lib::String();
	}
};


template<typename TPixelFormatType>
using RWTexture1DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, false>;

template<typename TPixelFormatType>
using RWTexture2DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, false>;

template<typename TPixelFormatType>
using RWTexture3DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, false>;


template<typename TPixelFormatType>
using OptionalRWTexture1DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, true>;

template<typename TPixelFormatType>
using OptionalRWTexture2DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, true>;

template<typename TPixelFormatType>
using OptionalRWTexture3DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, true>;

} // spt::gfx
