#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "RGResources/RGResourceHandles.h"
#include "DependenciesBuilder.h"

namespace spt::gfx
{

namespace priv
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Dimensions ====================================================================================

enum class EBindingTextureDimensions
{
	Dim_1D,
	Dim_2D,
	Dim_3D
};

template<EBindingTextureDimensions dimensions>
constexpr lib::String GetTextureDimSuffix()
{
	switch (dimensions)
	{
	case priv::EBindingTextureDimensions::Dim_1D:	return lib::String("1D");
	case priv::EBindingTextureDimensions::Dim_2D:	return lib::String("2D");
	case priv::EBindingTextureDimensions::Dim_3D:	return lib::String("3D");

	default:
		
		SPT_CHECK_NO_ENTRY(); // dimension not supported
		return lib::String();
	}
}


class TextureBindingBase abstract : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	explicit TextureBindingBase(const lib::HashedString& name)
		: Super(name)
	{ }

	void Set(lib::SharedPtr<rdr::TextureView> inTexture)
	{
		if (m_textureInstance != inTexture)
		{
			m_textureInstance = std::move(inTexture);
			m_renderGraphTexture.Reset();
			MarkAsDirty();
		}
	}

	void Set(rg::RGTextureViewHandle inTexture)
	{
		if (m_renderGraphTexture != inTexture)
		{
			m_renderGraphTexture = inTexture;
			m_textureInstance.reset();
			MarkAsDirty();
		}
	}

	void Reset()
	{
		m_textureInstance.reset();
		m_renderGraphTexture.Reset();
	}

	const lib::SharedPtr<rdr::TextureView>& GetTextureInstance() const
	{
		return m_textureInstance;
	}

	rg::RGTextureViewHandle GetRenderGraphTexture() const
	{
		return m_renderGraphTexture;
	}

	Bool IsValid() const
	{
		return !!m_textureInstance || m_renderGraphTexture.IsValid();
	}

protected:

	lib::SharedRef<rdr::TextureView> GetTextureToBind() const
	{
		SPT_CHECK(!m_textureInstance || !m_renderGraphTexture.IsValid()); // only one of these textures can be valid
		
		lib::SharedPtr<rdr::TextureView> textureView = m_textureInstance ? m_textureInstance : m_renderGraphTexture->GetViewInstance();
		SPT_CHECK(!!textureView);

		return lib::Ref(textureView);
	}

	void BuildRGDependenciesImpl(rg::RGDependenciesBuilder& builder, rg::ERGTextureAccess access) const
	{
		if (m_renderGraphTexture.IsValid())
		{
			builder.AddTextureAccess(m_renderGraphTexture, access, GetShaderStageFlags());
		}
		else
		{
			SPT_CHECK(!!m_textureInstance);
			builder.AddTextureAccess(lib::Ref(m_textureInstance), access, GetShaderStageFlags());
		}
	}

private:

	lib::SharedPtr<rdr::TextureView>	m_textureInstance;
	rg::RGTextureViewHandle				m_renderGraphTexture;
};

} // priv

} // spt::gfx
