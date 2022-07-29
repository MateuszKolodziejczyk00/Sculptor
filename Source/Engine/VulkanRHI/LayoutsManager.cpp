#include "LayoutsManager.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// TextureLayoutCommandBufferData ================================================================

TextureLayoutCommandBufferData::TextureLayoutCommandBufferData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout)
	: m_mipsNum(mipsNum)
	, m_arrayLayersNum(arrayLayers)
	, m_fullImageLayout(layout)
{ }

Bool TextureLayoutCommandBufferData::AreAllSubresourcesInSameLayout() const
{
	return m_subresourceLayouts.empty();
}

VkImageLayout TextureLayoutCommandBufferData::GetFullImageLayout() const
{
	SPT_CHECK(AreAllSubresourcesInSameLayout());

	return m_fullImageLayout;
}

VkImageLayout TextureLayoutCommandBufferData::GetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer) const
{
	return AreAllSubresourcesInSameLayout() ? GetFullImageLayout() : m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)];
}

VkImageLayout TextureLayoutCommandBufferData::GetSubresourcesSharedLayout(const TextureSubresourceRange& range) const
{
	SPT_PROFILE_FUNCTION();

	if (AreAllSubresourcesInSameLayout())
	{
		return GetFullImageLayout();
	}

	const Uint32 endMipLevel = range.m_baseMipLevel + range.m_mipLevelsNum;
	const Uint32 endArrayLayer = range.m_baseArrayLayer + range.m_arrayLayersNum;

	const VkImageLayout layout = m_subresourceLayouts[GetSubresourceIdx(range.m_baseMipLevel, range.m_baseArrayLayer)];

#if DO_CHECKS
	for (Uint32 mipLevel = range.m_baseMipLevel; mipLevel < endMipLevel; ++mipLevel)
	{
		for (Uint32 arrayLayer = range.m_baseArrayLayer; arrayLayer < endArrayLayer; ++arrayLayer)
		{
			SPT_CHECK(m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)] == layout);
		}
	}
#endif // TO_CHECK

	return layout;
}

void TextureLayoutCommandBufferData::SetFullImageLayout(VkImageLayout layout)
{
	m_fullImageLayout = layout;
	m_subresourceLayouts.clear();
}

void TextureLayoutCommandBufferData::SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout)
{
	SPT_CHECK(mipLevel < m_mipsNum && arrayLayer < m_arrayLayersNum);

	if (!AreAllSubresourcesInSameLayout())
	{
		TransitionToPerSubresourceLayout();
	}

	if (!AreAllSubresourcesInSameLayout())
	{
		m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)] = layout;

		TransitionToFullImageLayoutIfPossible();
	}
	else
	{
		// fallback if image has 1 mip level and 1 array layer
		m_fullImageLayout = m_fullImageLayout;
	}
}

void TextureLayoutCommandBufferData::SetSubresourcesLayout(const TextureSubresourceRange& range, VkImageLayout layout)
{
	SPT_PROFILE_FUNCTION();

	if (range.m_mipLevelsNum == 1 && range.m_arrayLayersNum == 1)
	{
		SetSubresourceLayout(range.m_baseMipLevel, range.m_baseArrayLayer, layout);
	}
	else
	{
		if (!AreAllSubresourcesInSameLayout())
		{
			TransitionToPerSubresourceLayout();
		}

		const Uint32 endMipLevel = range.m_baseMipLevel + range.m_mipLevelsNum;
		const Uint32 endArrayLayer = range.m_baseArrayLayer + range.m_arrayLayersNum;

		for (Uint32 mipLevel = range.m_baseMipLevel; mipLevel < endMipLevel; ++mipLevel)
		{
			for (Uint32 arrayLayer = range.m_baseArrayLayer; arrayLayer < endArrayLayer; ++arrayLayer)
			{
				m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)] = layout;
			}
		}

		TransitionToFullImageLayoutIfPossible();
	}
}

SizeType TextureLayoutCommandBufferData::GetSubresourceIdx(Uint32 mipLevel, Uint32 arrayLayer) const
{
	return static_cast<SizeType>(arrayLayer + mipLevel * m_arrayLayersNum);
}

void TextureLayoutCommandBufferData::TransitionToPerSubresourceLayout()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(AreAllSubresourcesInSameLayout()); // layout is currently stored as per-subresource

	if (m_mipsNum == 1 && m_arrayLayersNum == 1)
	{
		return;
	}

	m_subresourceLayouts.resize(m_mipsNum * m_arrayLayersNum);

	for (SizeType idx = 0; idx < m_subresourceLayouts.size(); ++idx)
	{
		m_subresourceLayouts[idx] = m_fullImageLayout;
	}
}

Bool TextureLayoutCommandBufferData::TransitionToFullImageLayoutIfPossible()
{
	if (m_subresourceLayouts.empty())
	{
		return true;
	}
	
	if (AreAllSubresourcesInSameLayoutImpl())
	{
		m_fullImageLayout = m_subresourceLayouts[0];
		m_subresourceLayouts.clear();
		return true;
	}

	return false;
}

Bool TextureLayoutCommandBufferData::AreAllSubresourcesInSameLayoutImpl() const
{
	SPT_PROFILE_FUNCTION();

	const VkImageLayout layout = m_subresourceLayouts[0];

	for (SizeType idx = 1; idx < m_subresourceLayouts.size(); ++idx)
	{
		if (m_subresourceLayouts[idx] != layout)
		{
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CommandBufferLayoutsManager ===================================================================

CommandBufferLayoutsManager::CommandBufferLayoutsManager()
{ }

void CommandBufferLayoutsManager::AcquireTexture(const TextureLayoutData& texture)
{
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// LayoutsManager ================================================================================

LayoutsManager::LayoutsManager()
{

}

}