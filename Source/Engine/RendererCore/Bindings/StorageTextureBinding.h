#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"


namespace spt::rdr
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType>
class StorageTextureBinding : public rdr::DescriptorSetBinding
{
public:

	explicit StorageTextureBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: rdr::DescriptorSetBinding(name, descriptorDirtyFlag)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateTexture(GetName(), lib::Ref(m_texture));
	}

	void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const
	{
		binding.Set(smd::TextureBindingData(1, GetBindingFlags()));
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		return BuildBindingVariableCode(lib::String("RWTexture") + priv::GetTextureDimSuffix<dimensions>()
										+ '<' + shader_translator::GetShaderTypeName<TPixelFormatType>() + "> " + name, bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return smd::EBindingFlags::Storage;
	}

	void Set(lib::SharedPtr<rdr::TextureView> inTexture)
	{
		if (m_texture != inTexture)
		{
			m_texture = std::move(inTexture);
			MarkAsDirty();
		}
	}

	void Reset()
	{
		m_texture.reset();
	}

	const lib::SharedPtr<rdr::TextureView>& Get() const
	{
		return m_texture;
	}

	Bool IsValid() const
	{
		return !!m_texture;
	}

private:

	lib::SharedPtr<rdr::TextureView> m_texture;
};


template<typename TPixelFormatType>
using StorageTexture1DBinding = StorageTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType>;

template<typename TPixelFormatType>
using StorageTexture2DBinding = StorageTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType>;

template<typename TPixelFormatType>
using StorageTexture3DBinding = StorageTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType>;

} // spt::rdr
