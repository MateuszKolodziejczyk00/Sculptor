#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Utility/String/StringUtils.h"
#include "Renderer.h"
#include "RendererSettings.h"

namespace spt::gfx
{

template<priv::EBindingTextureDimensions dimensions, SizeType arraySize, Bool trackInRenderGraph>
class ArrayOfSRVTexturesBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:

	static constexpr SizeType unboundSize = idxNone<SizeType>;

	explicit ArrayOfSRVTexturesBinding(const lib::HashedString& name)
		: Super(name)
	{
		std::generate_n(std::back_inserter(m_availableArrayIndices), arraySize, [ i = static_cast<Uint32>(arraySize) ]() mutable { return AvailableIndex{ --i, idxNone<Uint32> }; });
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
		for (const BoundTexture& boundTexture : m_boundTextures)
		{
			context.UpdateTexture(GetName(), boundTexture.texture, boundTexture.arrayIndex);
		}
	}
	
	void BuildRGDependencies(class rg::RGDependenciesBuilder& builder) const
	{
		if constexpr (trackInRenderGraph)
		{
			for (const BoundTexture& texture : m_boundTextures)
			{
				builder.AddTextureAccessIfAcquired(texture.texture, rg::ERGTextureAccess::SampledTexture);
			}
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String arraySizeNumber = IsUnbound() ? lib::String() : lib::StringUtils::ToString<arraySize>();
		return BuildBindingVariableCode(lib::String("Texture") + priv::GetTextureDimSuffix<dimensions>() + ' ' + name + '[' + arraySizeNumber + ']', bindingIdx);
	}

	static constexpr smd::EBindingFlags GetBindingFlags()
	{
		return lib::Flags(smd::EBindingFlags::PartiallyBound, smd::EBindingFlags::Unbound);
	}

	static constexpr Bool IsUnbound()
	{
		return arraySize == unboundSize;
	}

	Uint32 BindTexture(const lib::SharedRef<rdr::TextureView>& textureView)
	{
		SPT_CHECK(!m_availableArrayIndices.empty());

		const Uint64 currentFrameIdx	= rdr::Renderer::GetCurrentFrameIdx();
		const Uint64 framesInFlight		= rdr::RendererSettings::Get().framesInFlight;

		const auto foundAvailableIdxIt = std::find_if(std::crbegin(m_availableArrayIndices), std::crend(m_availableArrayIndices),
													  [ = ](const AvailableIndex& availableIdx)
													  {
														  return availableIdx.lastUseFrameIdx == idxNone<Uint32> || currentFrameIdx >= availableIdx.lastUseFrameIdx + framesInFlight;
													  });

		SPT_CHECK(foundAvailableIdxIt != std::crend(m_availableArrayIndices));
	
		const Uint32 arrayIndex = foundAvailableIdxIt->arrayIndex;
		m_availableArrayIndices.erase(std::next(foundAvailableIdxIt).base());
		m_boundTextures.emplace_back(BoundTexture{ textureView, arrayIndex });

		MarkAsDirty();

		return arrayIndex;
	}

	void UnbindTexture(Uint32 textureArrayIdx)
	{
		SPT_CHECK(textureArrayIdx != idxNone<Uint32>);
		SPT_CHECK(std::find_if(std::cbegin(m_availableArrayIndices), std::cend(m_availableArrayIndices), [textureArrayIdx](const AvailableIndex& idx) { return idx.arrayIndex == textureArrayIdx; }) == std::cend(m_availableArrayIndices));
		
		m_availableArrayIndices.emplace_back(AvailableIndex{ textureArrayIdx, rdr::Renderer::GetCurrentFrameIdx() });

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

	struct AvailableIndex
	{
		Uint32 arrayIndex;
		Uint64 lastUseFrameIdx;
	};

	lib::DynamicArray<AvailableIndex>	m_availableArrayIndices;
	lib::DynamicArray<BoundTexture>		m_boundTextures;
};


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTextures1DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_1D, arraySize, trackInRenderGraph>;


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTextures2DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_2D, arraySize, trackInRenderGraph>;

} // spt::gfx
