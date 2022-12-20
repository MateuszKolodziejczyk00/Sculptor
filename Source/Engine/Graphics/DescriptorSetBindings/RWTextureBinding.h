#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "DependenciesBuilder.h"


namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, typename TPixelFormatType>
class RWTextureBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit RWTextureBinding(const lib::HashedString& name, Bool& descriptorDirtyFlag)
		: Super(name, descriptorDirtyFlag)
	{ }

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		context.UpdateTexture(GetName(), GetTextureToBind());
	}
	
	void BuildRGDependencies(rg::RGDependenciesBuilder& builder)
	{
		if (m_renderGraphTexture)
		{
			builder.AddTextureAccess(m_renderGraphTexture, rg::ERGAccess::StorageWriteTexture);
		}
	}

	void CreateBindingMetaData(OUT smd::GenericShaderBinding& binding) const
	{
		binding.Set(smd::TextureBindingData(1, GetBindingFlags()));
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

	void Set(lib::SharedPtr<rdr::TextureView> inTexture)
	{
		if (m_texture != inTexture)
		{
			m_texture = std::move(inTexture);
			m_renderGraphTexture.Reset();
			MarkAsDirty();
		}
	}

	void Set(rg::RGTextureViewHandle inTexture)
	{
		if (m_renderGraphTexture != inTexture)
		{
			m_renderGraphTexture = inTexture;
			m_texture.reset();
			MarkAsDirty();
		}
	}

	void Reset()
	{
		m_texture.reset();
		m_renderGraphTexture.Reset();
	}

	const lib::SharedPtr<rdr::TextureView>& GetInstance() const
	{
		return m_texture;
	}

	const lib::SharedPtr<rdr::TextureView>& GetRenderGraphTexture() const
	{
		return m_renderGraphTexture;
	}

	Bool IsValid() const
	{
		return !!m_texture || m_renderGraphTexture.IsValid();
	}

private:

	lib::SharedRef<rdr::TextureView> GetTextureToBind() const
	{
		lib::SharedPtr<rdr::TextureView> textureView = m_texture ? m_texture : m_renderGraphTexture->GetViewInstance();
		SPT_CHECK(!!textureView);

		return lib::Ref(textureView);
	}

	lib::SharedPtr<rdr::TextureView>	m_texture;
	rg::RGTextureViewHandle				m_renderGraphTexture;
};


template<typename TPixelFormatType>
using RWTexture1DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType>;

template<typename TPixelFormatType>
using RWTexture2DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType>;

template<typename TPixelFormatType>
using RWTexture3DBinding = RWTextureBinding<priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType>;

} // spt::gfx
