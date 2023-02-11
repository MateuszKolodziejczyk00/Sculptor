#include "RGResourceTypes.h"
#include "RGNode.h"

namespace spt::rg
{
//////////////////////////////////////////////////////////////////////////////////////////////////
// RGTextureSubresourceAccessState ===============================================================

RGTextureSubresourceAccessState::RGTextureSubresourceAccessState()
	: lastAccessType(ERGTextureAccess::Unknown)
{ }

RGTextureSubresourceAccessState::RGTextureSubresourceAccessState(ERGTextureAccess inLastAccessType, RGNodeHandle inLastAccessNode)
	: lastAccessType(inLastAccessType)
	, lastAccessNode(inLastAccessNode)
{ }

Bool RGTextureSubresourceAccessState::CanMergeWith(const RGTextureSubresourceAccessState& other) const
{
	return lastAccessType == other.lastAccessType
		&& lastAccessNode->GetType() == other.lastAccessNode->GetType();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGTextureAccessState ==========================================================================

RGTextureAccessState::RGTextureAccessState(Uint32 textureMipsNum, Uint32 textureLayersNum)
	: m_textureMipsNum(textureMipsNum)
	, m_textureLayersNum(textureLayersNum)
{
	m_subresourcesAccesses.resize(1);
	m_subresourcesAccesses[0] = RGTextureSubresourceAccessState(ERGTextureAccess::Unknown, nullptr);
}

Bool RGTextureAccessState::IsFullResource() const
{
	return m_subresourcesAccesses.size() == 1;
}

RGTextureSubresourceAccessState& RGTextureAccessState::GetForFullResource()
{
	SPT_CHECK(IsFullResource());
	return m_subresourcesAccesses[0];
}

RGTextureSubresourceAccessState& RGTextureAccessState::GetForSubresource(RGTextureSubresource subresource)
{
	return GetForSubresource(subresource.arrayLayerIdx, subresource.mipMapIdx);
}

RGTextureSubresourceAccessState& RGTextureAccessState::GetForSubresource(Uint32 layerIdx, Uint32 mipMapIdx)
{
	SPT_CHECK(mipMapIdx < m_textureMipsNum&& layerIdx < m_textureLayersNum);

	if (IsFullResource())
	{
		return GetForFullResource();
	}

	return m_subresourcesAccesses[GetSubresourceIdx(layerIdx, mipMapIdx)];
}

void RGTextureAccessState::SetSubresourcesAccess(const RGTextureSubresourceAccessState& access, const rhi::TextureSubresourceRange& range)
{
	SPT_PROFILER_FUNCTION();

	if (RangeContainsFullResource(range))
	{
		MergeTo(access);
		return;
	}

	if (IsFullResource())
	{
		BreakIntoSubresources();
	}

	ForEachSubresource(range,
					   [this, &access](RGTextureSubresource subresource)
					   {
						   GetForSubresource(subresource) = access;
					   });

	TryMergeAccessState();
}

Bool RGTextureAccessState::RangeContainsFullResource(const rhi::TextureSubresourceRange& range)
{
	return range.baseMipLevel == 0
		&& range.baseArrayLayer == 0
		&& (range.mipLevelsNum == m_textureMipsNum || range.mipLevelsNum == rhi::constants::allRemainingMips)
		&& (range.arrayLayersNum == m_textureLayersNum  || range.arrayLayersNum == rhi::constants::allRemainingArrayLayers);
}

void RGTextureAccessState::MergeTo(const RGTextureSubresourceAccessState& access)
{
	m_subresourcesAccesses.resize(1);
	m_subresourcesAccesses[0] = access;
}

void RGTextureAccessState::BreakIntoSubresources()
{
	if (IsFullResource())
	{
		const RGTextureSubresourceAccessState state = m_subresourcesAccesses[0];
		m_subresourcesAccesses.resize(m_textureLayersNum * m_textureMipsNum);
		std::fill(std::begin(m_subresourcesAccesses), std::end(m_subresourcesAccesses), state);
	}
}

SizeType RGTextureAccessState::GetSubresourceIdx(Uint32 layerIdx, Uint32 mipMapIdx) const
{
	return static_cast<SizeType>(layerIdx * m_textureMipsNum + mipMapIdx);
}

Bool RGTextureAccessState::TryMergeAccessState()
{
	for (SizeType subressourceIdx = 1; subressourceIdx < m_subresourcesAccesses.size(); ++subressourceIdx)
	{
		if (!m_subresourcesAccesses[0].CanMergeWith(m_subresourcesAccesses[subressourceIdx]))
		{
			return false;
		}
	}

	m_subresourcesAccesses.resize(1);
	return true;
}

} // spt::rg
