#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "RGResourceHandles.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rg
{

enum class ERGTextureAccess
{
	Unknown,

	ColorRenderTarget,
	DepthRenderTarget,
	StencilRenderTarget,

	StorageWriteTexture,

	SampledTexture,

	TransferSource,
	TransferDest
};


enum class ERGBufferAccess
{
	Unknown,
	Read,
	Write,
	ReadWrite
};


enum class ERGResourceFlags : Flags32
{
	None = 0,
	External	= BIT(0),

	Default = None
};


struct RENDER_GRAPH_API RGTextureSubresourceAccessState
{
	RGTextureSubresourceAccessState();
	RGTextureSubresourceAccessState(ERGTextureAccess inLastAccessType, RGNodeHandle inLastAccessNode);

	Bool CanMergeWith(const RGTextureSubresourceAccessState& other) const;

	ERGTextureAccess	lastAccessType;
	RGNodeHandle		lastAccessNode{};
};


struct RGTextureSubresource
{
	explicit RGTextureSubresource(Uint32 inArrayLayerIdx, Uint32 inMipMapIdx)
		: arrayLayerIdx(inArrayLayerIdx)
		, mipMapIdx(inMipMapIdx)
	{ }

	Uint32 arrayLayerIdx;
	Uint32 mipMapIdx;
};


class RENDER_GRAPH_API RGTextureAccessState
{
public:

	RGTextureAccessState(Uint32 textureMipsNum, Uint32 textureLayersNum);

	Bool IsFullResource() const;

	RGTextureSubresourceAccessState& GetForFullResource();

	RGTextureSubresourceAccessState& GetForSubresource(RGTextureSubresource subresource);

	RGTextureSubresourceAccessState& GetForSubresource(Uint32 layerIdx, Uint32 mipMapIdx);

	template<typename TCallable>
	void ForEachSubresource(const rhi::TextureSubresourceRange& range, TCallable&& callable)
	{
		const Uint32 layersNum = range.arrayLayersNum == rhi::constants::allRemainingArrayLayers ? m_textureLayersNum : range.arrayLayersNum;
		const Uint32 mipsNum = range.mipLevelsNum == rhi::constants::allRemainingMips ? m_textureMipsNum : range.mipLevelsNum;

		const Uint32 lastLayerIdx = range.baseArrayLayer + layersNum;
		const Uint32 lastMipIdx = range.baseMipLevel + mipsNum;

		for (Uint32 layerIdx = range.baseArrayLayer; layerIdx < lastLayerIdx; ++layerIdx)
		{
			for (Uint32 mipIdx = range.baseMipLevel; mipIdx < lastMipIdx; ++mipIdx)
			{
				callable(RGTextureSubresource(layerIdx, mipIdx));
			}
		}
	}

	template<typename TCallable>
	void ForEachSubresource(TCallable&& callable)
	{
		for (Uint32 layerIdx = 0; layerIdx < m_textureLayersNum; ++layerIdx)
		{
			for (Uint32 mipIdx = 0; mipIdx < m_textureMipsNum; ++mipIdx)
			{
				callable(RGTextureSubresource(layerIdx, mipIdx));
			}
		}
	}

	void SetSubresourcesAccess(const RGTextureSubresourceAccessState& access, const rhi::TextureSubresourceRange& range);

	Bool RangeContainsFullResource(const rhi::TextureSubresourceRange& range);

	void MergeTo(const RGTextureSubresourceAccessState& access);

	void BreakIntoSubresources();

private:

	SizeType GetSubresourceIdx(Uint32 layerIdx, Uint32 mipMapIdx) const;

	Bool TryMergeAccessState();

	Uint32 m_textureMipsNum;
	Uint32 m_textureLayersNum;

	// For now we don't support different access masks
	lib::DynamicArray<RGTextureSubresourceAccessState> m_subresourcesAccesses;
};

} // spt::rg