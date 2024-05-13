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
			m_availableBlocks.insert(foundBlockIt, block);
		}
	}

private:

	lib::DynamicArray<TexturesBlock> m_availableBlocks;
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


template<priv::EBindingTextureDimensions dimensions, SizeType arraySize, Bool trackInRenderGraph>
class ArrayOfSRVTextureBlocksBinding : public rdr::DescriptorSetBinding
{
protected:

	using Super = rdr::DescriptorSetBinding;

public:
const 
	static constexpr SizeType unboundSize = idxNone<SizeType>;

	explicit ArrayOfSRVTextureBlocksBinding(const lib::HashedString& name)
		: Super(name)
		, m_textureBindingsAllocator(arraySize)
	{
		m_boundTextures.resize(arraySize);
	}

	virtual void UpdateDescriptors(rdr::DescriptorSetUpdateContext& context) const final
	{
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
				context.UpdateTexture(GetBaseBindingIdx(), lib::Ref(textureView), static_cast<Uint32>(arrayIdx));
			}
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
				else if (texture.rgTexture.IsValid())
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

	TexturesBindingsAllocationHandle AllocateTexturesBlock(Uint32 texturesNum)
	{
		SPT_CHECK(!IsUnbound());

		const auto allocatedBlock = m_textureBindingsAllocator.Allocate(texturesNum);
		return TexturesBindingsAllocationHandle(allocatedBlock.value());
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

	void BindTexture(const lib::SharedRef<rdr::TextureView>& textureView, TexturesBindingsAllocationHandle allocation, Uint32 offset)
	{
		SPT_CHECK(offset < allocation.GetSize());

		const Uint32 textureArrayIdx = allocation.GetOffset() + offset;

		BoundTexture textureToBind;
		textureToBind.textureInstance = textureView;
		return BindTextureImpl(textureToBind, textureArrayIdx);
	}

	void BindTexture(const rg::RGTextureViewHandle& textureView, TexturesBindingsAllocationHandle allocation, Uint32 offset)
	{
		SPT_CHECK(trackInRenderGraph);

		const Uint32 textureArrayIdx = allocation.GetOffset() + offset;

		BoundTexture textureToBind;
		textureToBind.rgTexture = textureView;
		return BindTextureImpl(textureToBind, textureArrayIdx);
	}

	void UnbindTexture(TexturesBindingsAllocationHandle allocation, Uint32 offset)
	{
		SPT_CHECK(offset != idxNone<Uint32>);

		const Uint32 textureArrayIdx = allocation.GetOffset() + offset;
		
		m_boundTextures[textureArrayIdx] = BoundTexture{};
		MarkAsDirty();
	}

	std::variant<nullptr_t, lib::SharedPtr<rdr::TextureView>, rg::RGTextureViewHandle> GetBoundTexture(Uint32 textureArrayIdx) const
	{
		SPT_CHECK(textureArrayIdx != idxNone<Uint32>);

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

	lib::DynamicArray<BoundTexture> m_boundTextures;
	priv::TexturesBindingAllocator  m_textureBindingsAllocator;
};


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture1DBlocksBinding = ArrayOfSRVTextureBlocksBinding<priv::EBindingTextureDimensions::Dim_1D, arraySize, trackInRenderGraph>;


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture2DBlocksBinding = ArrayOfSRVTextureBlocksBinding<priv::EBindingTextureDimensions::Dim_2D, arraySize, trackInRenderGraph>;


template<SizeType arraySize, Bool trackInRenderGraph = false>
using ArrayOfSRVTexture3DBlocksBinding = ArrayOfSRVTextureBlocksBinding<priv::EBindingTextureDimensions::Dim_3D, arraySize, trackInRenderGraph>;

} // spt::gfx
