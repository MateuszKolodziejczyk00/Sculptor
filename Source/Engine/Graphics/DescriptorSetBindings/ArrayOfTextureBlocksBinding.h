#pragma once

#include "SculptorCoreTypes.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "TextureBindingTypes.h"
#include "ShaderStructs/ShaderStructsTypes.h"
#include "Utility/String/StringUtils.h"
#include "Renderer.h"
#include "RendererSettings.h"
#include "ShaderStructs/ShaderStructsTypes.h"


namespace spt::gfx
{

namespace priv
{

class TexturesBindingAllocator
{
public:

	explicit TexturesBindingAllocator(Uint32 arraySize)
	{
		m_availableBlocks.emplace_back(TexturesBlock{ 0u, arraySize });
	}

	struct TexturesBlock
	{
		TexturesBlock() = default;

		TexturesBlock(Uint32 inBlockStart, Uint32 inBlockSize)
			: blockStart(inBlockStart)
			, blockSize(inBlockSize)
		{
		}

		Bool operator<(const TexturesBlock& rhs) const
		{
			return blockStart < rhs.blockStart;
		}

		Uint32 blockStart = 0u;
		Uint32 blockSize  = 0u;
	};

	std::optional<TexturesBlock> Allocate(Uint32 texturesNum)
	{
		const auto foundBlockIt = std::find_if(std::begin(m_availableBlocks), std::end(m_availableBlocks),
											   [=](const TexturesBlock& block)
											   {
												   return block.blockSize >= texturesNum;
											   });

		if (foundBlockIt != std::end(m_availableBlocks))
		{
			const Uint32 blockStart = foundBlockIt->blockStart;
			const Uint32 blockSize  = foundBlockIt->blockSize;

			if (blockSize == texturesNum)
			{
				m_availableBlocks.erase(foundBlockIt);
			}
			else
			{
				foundBlockIt->blockStart += texturesNum;
				foundBlockIt->blockSize -= texturesNum;
			}

			return TexturesBlock{ blockStart, texturesNum };
		}

		return std::nullopt;

	}

	void Deallocate(TexturesBlock block)
	{
		const auto foundBlockIt = std::lower_bound(std::begin(m_availableBlocks), std::end(m_availableBlocks), block);

		if (foundBlockIt != std::end(m_availableBlocks) && foundBlockIt->blockStart == block.blockStart + block.blockSize)
		{
			foundBlockIt->blockStart = block.blockStart;
			foundBlockIt->blockSize += block.blockSize;
		}
		else
		{
			if (foundBlockIt != std::begin(m_availableBlocks))
			{
				const auto prevBlockIt = std::prev(foundBlockIt);
				if (prevBlockIt->blockStart + prevBlockIt->blockSize == block.blockStart)
				{
					prevBlockIt->blockSize += block.blockSize;
					return;
				}
			}

			m_availableBlocks.insert(foundBlockIt, block);
		}
	}

private:

	lib::DynamicArray<TexturesBlock> m_availableBlocks;
};


enum class ETextureResourceType
{
	SRVTexture,
	RWTexture
};

} // priv


class TexturesBindingsAllocationHandle
{
public:

	TexturesBindingsAllocationHandle() = default;

	explicit TexturesBindingsAllocationHandle(priv::TexturesBindingAllocator::TexturesBlock allocatedBlock)
		: m_allocatedBlock(allocatedBlock)
	{
	}

	Bool IsValid() const
	{
		return m_allocatedBlock.blockSize != 0u;
	}

	Uint32 GetOffset() const
	{
		return m_allocatedBlock.blockStart;
	}

	Uint32 GetSize() const
	{
		return m_allocatedBlock.blockSize;
	}

	void Reset()
	{
		m_allocatedBlock = priv::TexturesBindingAllocator::TexturesBlock{};
	}

	const priv::TexturesBindingAllocator::TexturesBlock& GetAllocatedBlock() const
	{
		return m_allocatedBlock;
	}

private:

	priv::TexturesBindingAllocator::TexturesBlock m_allocatedBlock;
};


template<priv::ETextureResourceType resourceType, priv::EBindingTextureDimensions dimensions, typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph>
class ArrayOfTextureBlocksBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:
const 
	static constexpr SizeType unboundSize = idxNone<SizeType>;

	explicit ArrayOfTextureBlocksBinding(const lib::HashedString& name)
		: Super(name)
		, m_textureBindingsAllocator(arraySize)
	{
		m_boundTextures.resize(arraySize);
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetIndexer& indexer) const final
	{
		SPT_PROFILER_FUNCTION();

		for (SizeType arrayIdx = 0; arrayIdx < m_boundTextures.size(); ++arrayIdx)
		{
			const BoundTexture& boundTexture = m_boundTextures[arrayIdx];

			lib::SharedPtr<rdr::TextureView> textureView = boundTexture.textureInstance;

			if (!textureView && boundTexture.rgTexture.IsValid())
			{
				textureView = boundTexture.rgTexture->GetViewInstance();
			} 

			if (textureView)
			{
				textureView->GetRHI().CopySRVDescriptor(indexer[GetBaseBindingIdx()][static_cast<Uint32>(arrayIdx)]);
			}
		}
	}

	void BuildRGDependencies(class rg::RGDependenciesBuilder& builder) const
	{
		if constexpr (trackInRenderGraph)
		{
			constexpr rg::ERGTextureAccess accessType = GetRGTextureAccessType();
			for (const BoundTexture& texture : m_boundTextures)
			{
				if (texture.textureInstance)
				{
					builder.AddTextureAccessIfAcquired(lib::Ref(texture.textureInstance), accessType);
				}
				else if (texture.rgTexture.IsValid())
				{
					builder.AddTextureAccess(texture.rgTexture, accessType);
				}
			}
		}
	}

	static constexpr lib::String BuildBindingCode(const char* name, Uint32 bindingIdx)
	{
		const lib::String arraySizeNumber = IsUnbound() ? lib::String() : lib::StringUtils::ToString<arraySize>();

		SPT_STATIC_CHECK(SPT_SINGLE_ARG(resourceType != priv::ETextureResourceType::RWTexture || std::is_same_v<TPixelFormatType, math::Vector4f>));

		return BuildBindingVariableCode(GetTextureTypeString() + priv::GetTextureDimSuffix<dimensions>() + 
										'<' + rdr::shader_translator::GetShaderTypeName<TPixelFormatType>() + '>'
										+ ' ' + name + '[' + arraySizeNumber + ']', bindingIdx);
	}

	static constexpr std::array<rdr::ShaderBindingMetaData, 1> GetShaderBindingsMetaData()
	{
		if constexpr (resourceType == priv::ETextureResourceType::SRVTexture)
		{
			return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::SampledTexture, lib::Flags(smd::EBindingFlags::PartiallyBound, smd::EBindingFlags::Unbound), arraySize) };
		}
		else if constexpr (resourceType == priv::ETextureResourceType::RWTexture)
		{
			return { rdr::ShaderBindingMetaData(rhi::EDescriptorType::StorageTexture, lib::Flags(smd::EBindingFlags::PartiallyBound, smd::EBindingFlags::Unbound), arraySize) };
		}
		else
		{
			return {};
		}
	}

	static constexpr Bool IsUnbound()
	{
		return arraySize == unboundSize;
	}

	TexturesBindingsAllocationHandle AllocateTexturesBlock(Uint32 texturesNum)
	{
		SPT_CHECK(!IsUnbound());

		const auto allocatedBlock = m_textureBindingsAllocator.Allocate(texturesNum);
		return TexturesBindingsAllocationHandle(allocatedBlock.value());
	}

	TexturesBindingsAllocationHandle AllocateWholeBlock()
	{
		const auto blockAllocation = AllocateTexturesBlock(arraySize);
		SPT_CHECK(blockAllocation.GetOffset() == 0u);
		return blockAllocation;
	}

	void DeallocateTexturesBlock(TexturesBindingsAllocationHandle allocationHandle)
	{
		SPT_CHECK(!IsUnbound());
		SPT_CHECK(allocationHandle.IsValid());
		SPT_CHECK(AreAllTexturesUnbound(allocationHandle));

		m_textureBindingsAllocator.Deallocate(allocationHandle.GetAllocatedBlock());
	}

	void UnbindAndDeallocateTexturesBlock(TexturesBindingsAllocationHandle allocationHandle)
	{
		for (Uint32 localIdx = 0; localIdx < allocationHandle.GetSize(); ++localIdx)
		{
			UnbindTexture(allocationHandle, localIdx);
		}

		DeallocateTexturesBlock(allocationHandle);
	}

	void BindTexture(const lib::SharedRef<rdr::TextureView>& textureView, TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx)
	{
		SPT_CHECK(allocation.IsValid());
		SPT_CHECK(allocationLocalIdx < allocation.GetSize());

		const Uint32 textureArrayIdx = allocation.GetOffset() + allocationLocalIdx;

		BoundTexture textureToBind;
		textureToBind.textureInstance = textureView;
		return BindTextureImpl(textureToBind, textureArrayIdx);
	}

	void BindTexture(const rg::RGTextureViewHandle& textureView, TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx)
	{
		SPT_CHECK(trackInRenderGraph);

		const Uint32 textureArrayIdx = allocation.GetOffset() + allocationLocalIdx;

		BoundTexture textureToBind;
		textureToBind.rgTexture = textureView;
		return BindTextureImpl(textureToBind, textureArrayIdx);
	}

	void BindTextures(lib::Span<const rg::RGTextureViewHandle> textureViews, TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx)
	{
		const Uint32 texturesNum = static_cast<Uint32>(textureViews.size());
		for (Uint32 textureIdx = 0u; textureIdx < texturesNum; ++textureIdx)
		{
			BindTexture(textureViews[textureIdx], allocation, allocationLocalIdx + textureIdx);
		}
	}

	void UnbindTexture(TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx)
	{
		SPT_CHECK(allocation.IsValid());
		SPT_CHECK(allocationLocalIdx < allocation.GetSize());

		const Uint32 textureArrayIdx = allocation.GetOffset() + allocationLocalIdx;
		
		m_boundTextures[textureArrayIdx] = BoundTexture{};
		MarkAsDirty();
	}

	std::variant<nullptr_t, lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle> GetBoundTexture(TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx) const
	{
		SPT_CHECK(allocation.IsValid());
		SPT_CHECK(allocationLocalIdx < allocation.GetSize());

		const Uint32 textureArrayIdx = allocation.GetOffset() + allocationLocalIdx;

		const BoundTexture& boundTexture = m_boundTextures[textureArrayIdx];

		if(boundTexture.textureInstance)
		{
			return boundTexture.textureInstance;
		}
		else if (boundTexture.rgTexture.IsValid())
		{
			return boundTexture.rgTexture;
		}
		else
		{
			return nullptr;
		}
	}

	template<typename TBoundTextureType>
	TBoundTextureType GetBoundTexture(TexturesBindingsAllocationHandle allocation, Uint32 allocationLocalIdx) const
	{
		const auto boundTexture = GetBoundTexture(allocation, allocationLocalIdx);
		return std::holds_alternative<TBoundTextureType>(boundTexture) ? std::get<TBoundTextureType>(boundTexture) : TBoundTextureType{};
	}

private:

	struct BoundTexture
	{
		lib::SharedPtr<rdr::TextureView> textureInstance;
		rg::RGTextureViewHandle rgTexture;
	};

	void BindTextureImpl(BoundTexture textureToBind, Uint32 idx)
	{
		m_boundTextures[idx] = textureToBind;

		MarkAsDirty();
	}

	Bool AreAllTexturesUnbound(TexturesBindingsAllocationHandle allocation) const
	{
		const Uint32 offset = allocation.GetOffset();
		const Uint32 size   = allocation.GetSize();

		for (Uint32 i = offset; i < offset + size; ++i)
		{
			if (m_boundTextures[i].textureInstance || m_boundTextures[i].rgTexture.IsValid())
			{
				return false;
			}
		}

		return true;
	}

	static constexpr rg::ERGTextureAccess GetRGTextureAccessType()
	{
		if constexpr (resourceType == priv::ETextureResourceType::SRVTexture)
		{
			return rg::ERGTextureAccess::SampledTexture;
		}
		else if constexpr (resourceType == priv::ETextureResourceType::RWTexture)
		{
			return rg::ERGTextureAccess::StorageWriteTexture;
		}
		else
		{
			return rg::ERGTextureAccess::SampledTexture;
		}
	}

	static constexpr lib::String GetTextureTypeString()
	{
		if constexpr (resourceType == priv::ETextureResourceType::SRVTexture)
		{
			return "Texture";
		}
		else if constexpr (resourceType == priv::ETextureResourceType::RWTexture)
		{
			return "RWTexture";
		}
		else
		{
			return "UnknownTextureType";
		}
	}

	lib::DynamicArray<BoundTexture> m_boundTextures;
	priv::TexturesBindingAllocator  m_textureBindingsAllocator;
};


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture1DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::SRVTexture, priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, arraySize, trackInRenderGraph>;


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture2DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::SRVTexture, priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, arraySize, trackInRenderGraph>;


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture3DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::SRVTexture, priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, arraySize, trackInRenderGraph>;


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = true>
using ArrayOfRWTexture1DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::RWTexture, priv::EBindingTextureDimensions::Dim_1D, TPixelFormatType, arraySize, trackInRenderGraph>;


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = true>
using ArrayOfRWTexture2DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::RWTexture, priv::EBindingTextureDimensions::Dim_2D, TPixelFormatType, arraySize, trackInRenderGraph>;


template<typename TPixelFormatType, SizeType arraySize, Bool trackInRenderGraph = true>
using ArrayOfRWTexture3DBlocksBinding = ArrayOfTextureBlocksBinding<priv::ETextureResourceType::RWTexture, priv::EBindingTextureDimensions::Dim_3D, TPixelFormatType, arraySize, trackInRenderGraph>;

} // spt::gfx
