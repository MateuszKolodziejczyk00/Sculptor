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
		std::generate_n(std::back_inserter(m_availableArrayIndices), arraySize, [ i = static_cast<Uint32>(arraySize) ]() mutable { return AvailableIndex{ --i, rdr::GPUTimelineSection() }; });
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		for (const BoundTexture& boundTexture : m_boundTextures)
		{
			const lib::SharedPtr<rdr::TextureView> textureView = boundTexture.textureInstance ? boundTexture.textureInstance : boundTexture.rgTexture->GetViewInstance();
			textureView->GetRHI().CopySRVDescriptor(indexer[GetBaseBindingIdx()][boundTexture.arrayIndex]);
		}
	}

	void BuildRGDependencies(class rg::RGDependenciesBuilder& builder) const
	{
		if constexpr (trackInRenderGraph)
		{
			for (const BoundTexture& texture : m_boundTextures)
			{
				if (texture.textureInstance)
				{
					builder.AddTextureAccessIfAcquired(lib::Ref(texture.textureInstance), rg::ERGTextureAccess::SampledTexture);
				}
				else
				{
					builder.AddTextureAccess(texture.rgTexture, rg::ERGTextureAccess::SampledTexture);
				}
			}
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String arraySizeNumber = IsUnbound() ? lib::String() : lib::StringUtils::ToString<arraySize>();
		return BuildBindingVariableCode(lib::String("Texture") + priv::GetTextureDimSuffix<dimensions>() + ' ' + name + '[' + arraySizeNumber + ']', bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::SampledTexture, lib::Flags(smd::EBindingFlags::PartiallyBound, smd::EBindingFlags::Unbound), arraySize) };
	}

	static constexpr Bool IsUnbound()
	{
		return arraySize == unboundSize;
	}

	Uint32 BindTexture(const lib::SharedRef<rdr::TextureView>& textureView)
	{
		BoundTexture textureToBind;
		textureToBind.textureInstance = textureView;
		return BindTextureImpl(textureToBind);
	}

	Uint32 BindTexture(const rg::RGTextureViewHandle& textureView)
	{
		SPT_CHECK(trackInRenderGraph);

		BoundTexture textureToBind;
		textureToBind.rgTexture = textureView;
		return BindTextureImpl(textureToBind);
	}

	void UnbindTexture(Uint32 textureArrayIdx)
	{
		SPT_CHECK(textureArrayIdx != idxNone<Uint32>);
		SPT_CHECK(std::find_if(std::cbegin(m_availableArrayIndices), std::cend(m_availableArrayIndices), [textureArrayIdx](const AvailableIndex& idx) { return idx.arrayIndex == textureArrayIdx; }) == std::cend(m_availableArrayIndices));
		
		m_availableArrayIndices.emplace_back(AvailableIndex{ textureArrayIdx, rdr::Renderer::GetDeviceQueuesManager().GetRecordedSection() });

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

	std::variant<nullptr_t, lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle> GetBoundTexture(Uint32 textureArrayIdx) const
	{
		SPT_CHECK(textureArrayIdx != idxNone<Uint32>);

		const auto boundTextureIt = std::find_if(std::cbegin(m_boundTextures), std::cend(m_boundTextures),
												 [textureArrayIdx](const BoundTexture& boundTexture)
												 {
													 return boundTexture.arrayIndex == textureArrayIdx;
												 });

		if (boundTextureIt != std::cend(m_boundTextures))
		{
			if(boundTextureIt->textureInstance)
			{
				return boundTextureIt->textureInstance;
			}
			else
			{
				return boundTextureIt->rgTexture;
			}
		}
		else
		{
			return nullptr;
		}
	}

	template<typename TBoundTextureType>
	TBoundTextureType GetBoundTexture(Uint32 textureArrayUdx) const
	{
		const auto boundTexture = GetBoundTexture(textureArrayUdx);
		return std::holds_alternative<TBoundTextureType>(boundTexture) ? std::get<TBoundTextureType>(boundTexture) : TBoundTextureType{};
	}

private:

	struct BoundTexture
	{
		lib::SharedPtr<rdr::TextureView> textureInstance;
		rg::RGTextureViewHandle rgTexture;
		Uint32 arrayIndex;
	};

	struct AvailableIndex
	{
		Uint32 arrayIndex;
		rdr::GPUTimelineSection lastUseGPUSection;
	};

	Uint32 BindTextureImpl(BoundTexture textureToBind)
	{
		SPT_CHECK(!m_availableArrayIndices.empty());
		SPT_CHECK(textureToBind.rgTexture.IsValid() || !!textureToBind.textureInstance);

		const auto foundAvailableIdxIt = std::find_if(std::crbegin(m_availableArrayIndices), std::crend(m_availableArrayIndices),
													  [ = ](const AvailableIndex& availableIdx)
													  {
														  return rdr::Renderer::GetDeviceQueuesManager().IsExecuted(availableIdx.lastUseGPUSection);
													  });

		SPT_CHECK(foundAvailableIdxIt != std::crend(m_availableArrayIndices));
	
		const Uint32 arrayIndex = foundAvailableIdxIt->arrayIndex;
		m_availableArrayIndices.erase(std::next(foundAvailableIdxIt).base());
		textureToBind.arrayIndex = arrayIndex;
		m_boundTextures.emplace_back(textureToBind);

		MarkAsDirty();

		return arrayIndex;
	}

	lib::DynamicArray<AvailableIndex>	m_availableArrayIndices;
	lib::DynamicArray<BoundTexture>		m_boundTextures;
};


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTextures1DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_1D, arraySize, trackInRenderGraph>;


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTextures2DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_2D, arraySize, trackInRenderGraph>;


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTextures3DBinding = ArrayOfSRVTexturesBinding<priv::EBindingTextureDimensions::Dim_3D, arraySize, trackInRenderGraph>;

} // spt::gfx
