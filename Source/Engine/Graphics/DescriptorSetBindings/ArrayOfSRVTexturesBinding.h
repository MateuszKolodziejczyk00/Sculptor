#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Utility/String/StringUtils.h"

namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, SizeType arraySize>
class ArrayOfSRVTexturesBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	static constexpr SizeType unboundSize = idxNone<SizeType>;

	explicit ArrayOfSRVTexturesBinding(const lib::HashedString& name)
		: Super(name)
	{
		std::generate_n(std::back_inserter(m_availableArrayIndices), arraySize, [i = 0]() mutable { return i++; });
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		for (const BoundTexture& boundTexture : m_boundTextures)
		{
			context.UpdateTexture(GetName(), boundTexture.texture, boundTexture.arrayIndex);
		}
	}
	
	// No dependencies by design
	// this binding should store only constant textures, so we don't need barriers etc.
	void BuildRGDependencies(class rg::RGDependenciesBuilder& builder) const
	{ }

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String arraySizeNumber = IsUnbound() ? lib::String() : lib::StringUtils::ToString<arraySize>();
		return BuildBindingVariableCode(lib::String("Texture") + priv::GetTextureDimSuffix<dimensions>() + name + '[' + arraySizeNumber + ']', bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return IsUnbound() ? smd::EBindingFlags::Unbound : smd::EBindingFlags::None;
	}

	static constexpr Bool IsUnbound()
	{
		return arraySize == unboundSize;
	}

	Uint32 BindTexture(const lib::SharedRef<rdr::TextureView>& textureView)
	{
		SPT_CHECK(!m_availableArrayIndices.empty());

		const Uint32 arrayIndex = m_availableArrayIndices.back();
		m_availableArrayIndices.pop_back();
		m_boundTextures.emplace_back(BoundTexture{ textureView, arrayIndex });

		MarkAsDirty();

		return arrayIndex;
	}

	void UnbindTexture(Uint32 textureArrayIdx)
	{
		SPT_CHECK(textureArrayIdx != idxNone<Uint32>);
		SPT_CHECK(std::find(std::cbegin(m_availableArrayIndices), std::cend(m_availableArrayIndices), textureArrayIdx) == std::cend(m_availableArrayIndices));
		m_availableArrayIndices.emplace(textureArrayIdx);

		const auto boundTextureIt = std::find_if(std::cbegin(m_boundTextures), std::cend(m_boundTextures),
												 [textureArrayIdx](const BoundTexture& boundTexture)
												 {
													 return boundTexture.arrayIndex == textureArrayIdx;
												 });

		if (boundTextureIt != std::cend(m_boundTextures))
		{
			m_boundTextures.erase(boundTextureIt);
		}
	}

private:

	struct BoundTexture
	{
		lib::SharedRef<rdr::TextureView> texture;
		Uint32 arrayIndex;
	};

	lib::DynamicArray<Uint32>		m_availableArrayIndices;
	lib::DynamicArray<BoundTexture>	m_boundTextures;
};


template<SizeType arraySize>
using ArrayOfSRVTextures1DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_1D, arraySize>;


template<SizeType arraySize>
using ArrayOfSRVTextures2DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_2D, arraySize>;

} // spt::gfx
