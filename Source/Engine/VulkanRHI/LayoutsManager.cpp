#include "LayoutsManager.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSubresourcesLayoutsData ==================================================================

ImageSubresourcesLayoutsData::ImageSubresourcesLayoutsData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout)
	: m_mipsNum(mipsNum)
	, m_arrayLayersNum(arrayLayers)
	, m_fullImageLayout(layout)
{ }

Bool ImageSubresourcesLayoutsData::AreAllSubresourcesInSameLayout() const
{
	return m_subresourceLayouts.empty();
}

VkImageLayout ImageSubresourcesLayoutsData::GetFullImageLayout() const
{
	SPT_CHECK(AreAllSubresourcesInSameLayout());

	return m_fullImageLayout;
}

VkImageLayout ImageSubresourcesLayoutsData::GetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer) const
{
	return AreAllSubresourcesInSameLayout() ? GetFullImageLayout() : m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)];
}

VkImageLayout ImageSubresourcesLayoutsData::GetSubresourcesSharedLayout(const ImageSubresourceRange& range) const
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

void ImageSubresourcesLayoutsData::SetFullImageLayout(VkImageLayout layout)
{
	m_fullImageLayout = layout;
	m_subresourceLayouts.clear();
}

void ImageSubresourcesLayoutsData::SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout)
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

void ImageSubresourcesLayoutsData::SetSubresourcesLayout(const ImageSubresourceRange& range, VkImageLayout layout)
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

SizeType ImageSubresourcesLayoutsData::GetSubresourceIdx(Uint32 mipLevel, Uint32 arrayLayer) const
{
	return static_cast<SizeType>(arrayLayer + mipLevel * m_arrayLayersNum);
}

void ImageSubresourcesLayoutsData::TransitionToPerSubresourceLayout()
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

Bool ImageSubresourcesLayoutsData::TransitionToFullImageLayoutIfPossible()
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

Bool ImageSubresourcesLayoutsData::AreAllSubresourcesInSameLayoutImpl() const
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

ImageSubresourcesLayoutsData* CommandBufferLayoutsManager::AcquireImage(VkImage image, const ImageLayoutData& layoutInfo)
{
	const ImageSubresourcesLayoutsData acquiredImageLayoutData(layoutInfo.m_mipsNum, layoutInfo.m_arrayLayersNum, layoutInfo.m_fullImageLayout);

	auto emplaceResult = m_imageLayouts.emplace(image, acquiredImageLayoutData);
	return &(emplaceResult.first->second);
}

const CommandBufferLayoutsManager::ImagesLayoutData& CommandBufferLayoutsManager::GetAcquiredImagesLayouts() const
{
	return m_imageLayouts;
}

ImageSubresourcesLayoutsData* CommandBufferLayoutsManager::GetImageLayoutInfo(VkImage image)
{
	auto foundLayout = m_imageLayouts.find(image);
	return foundLayout != m_imageLayouts.cend() ? &(foundLayout->second) : nullptr;
}

const ImageSubresourcesLayoutsData* CommandBufferLayoutsManager::GetImageLayoutInfo(VkImage image) const
{
	const auto foundLayout = m_imageLayouts.find(image);
	return foundLayout != m_imageLayouts.cend() ? &(foundLayout->second) : nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// LayoutsManager ================================================================================

LayoutsManager::LayoutsManager()
{ }

void LayoutsManager::RegisterImage(VkImage image, Uint32 mipsNum, Uint32 arrayLayersNum)
{
	SPT_PROFILE_FUNCTION();

	const lib::WriteLockGuard imageLayoutsWriteLock(m_imageLayoutsLock);

	m_imageLayouts.emplace(image, ImageLayoutData(mipsNum, arrayLayersNum));
}

void LayoutsManager::UnregisterImage(VkImage image)
{
	SPT_PROFILE_FUNCTION();

	const lib::WriteLockGuard imageLayoutsWriteLock(m_imageLayoutsLock);

	m_imageLayouts.erase(image);
}

VkImageLayout LayoutsManager::GetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image) const
{
	SPT_PROFILE_FUNCTION();

	const ImageSubresourcesLayoutsData* layoutData = GetImageLayoutDataIfAcquired(cmdBuffer, image);

	VkImageLayout result = VK_IMAGE_LAYOUT_UNDEFINED;

	if (layoutData)
	{
		result = layoutData->GetFullImageLayout();
	}
	else
	{
		result = GetGlobalFullImageLayout(image);
	}

	return result;
}

VkImageLayout LayoutsManager::GetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer) const
{
	SPT_PROFILE_FUNCTION();

	const ImageSubresourcesLayoutsData* layoutData = GetImageLayoutDataIfAcquired(cmdBuffer, image);

	VkImageLayout result = VK_IMAGE_LAYOUT_UNDEFINED;

	if (layoutData)
	{
		result = layoutData->GetSubresourceLayout(mipLevel, arrayLayer);
	}
	else
	{
		result = GetGlobalFullImageLayout(image);
	}

	return result;
}

VkImageLayout LayoutsManager::GetSubresourcesSharedLayout(VkCommandBuffer cmdBuffer, VkImage image, const ImageSubresourceRange& range) const
{
	SPT_PROFILE_FUNCTION();

	const ImageSubresourcesLayoutsData* layoutData = GetImageLayoutDataIfAcquired(cmdBuffer, image);

	VkImageLayout result = VK_IMAGE_LAYOUT_UNDEFINED;

	if (layoutData)
	{
		result = layoutData->GetSubresourcesSharedLayout(range);
	}
	else
	{
		result = GetGlobalFullImageLayout(image);
	}

	return result;
}

void LayoutsManager::SetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout layout)
{
	ImageSubresourcesLayoutsData& layoutData = GetAcquiredImageLayoutData(cmdBuffer, image);
	layoutData.SetFullImageLayout(layout);
}

void LayoutsManager::SetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout)
{
	ImageSubresourcesLayoutsData& layoutData = GetAcquiredImageLayoutData(cmdBuffer, image);
	layoutData.SetSubresourceLayout(mipLevel, arrayLayer, layout);
}

void LayoutsManager::SetSubresourcesLayout(VkCommandBuffer cmdBuffer, VkImage image, const ImageSubresourceRange& range, VkImageLayout layout)
{
	ImageSubresourcesLayoutsData& layoutData = GetAcquiredImageLayoutData(cmdBuffer, image);
	layoutData.SetSubresourcesLayout(range, layout);
}

void LayoutsManager::RegisterRecordingCommandBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	const lib::WriteLockGuard cmdBuifferLayoutsManagersWriteGuard(m_cmdBuffersLayoutManagersLock);

	m_cmdBuffersLayoutManagers.emplace(cmdBuffer, std::make_unique<CommandBufferLayoutsManager>());
}

void LayoutsManager::UnregisterRecordingCommnadBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	ReleaseCommandBufferResources(cmdBuffer);

	const lib::WriteLockGuard cmdBuifferLayoutsManagersWriteGuard(m_cmdBuffersLayoutManagersLock);

	m_cmdBuffersLayoutManagers.erase(cmdBuffer);
}

ImageSubresourcesLayoutsData& LayoutsManager::GetAcquiredImageLayoutData(VkCommandBuffer cmdBuffer, VkImage image)
{
	CommandBufferLayoutsManager& cmdBufferLayoutsManager = GetLayoutsManagerForCommandBuffer(cmdBuffer);

	ImageSubresourcesLayoutsData* layoutData = cmdBufferLayoutsManager.GetImageLayoutInfo(image);
	if (!layoutData)
	{
		layoutData = AcquireImage(cmdBufferLayoutsManager, image);
	}
	SPT_CHECK(!!layoutData);
	
	return *layoutData;
}

const ImageSubresourcesLayoutsData* LayoutsManager::GetImageLayoutDataIfAcquired(VkCommandBuffer cmdBuffer, VkImage image) const
{
	const CommandBufferLayoutsManager& cmdBufferLayoutsManager = GetLayoutsManagerForCommandBuffer(cmdBuffer);

	return cmdBufferLayoutsManager.GetImageLayoutInfo(image);
}

ImageSubresourcesLayoutsData* LayoutsManager::AcquireImage(CommandBufferLayoutsManager& cmdBufferLayoutsManager, VkImage image)
{
	SPT_PROFILE_FUNCTION();

	ImageLayoutData imageLayoutData;

	{
		const lib::ReadLockGuard imageLayoutsReadGuard(m_imageLayoutsLock);

		imageLayoutData = m_imageLayouts[image];
	}

	return cmdBufferLayoutsManager.AcquireImage(image, imageLayoutData);
}

void LayoutsManager::ReleaseCommandBufferResources(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	const CommandBufferLayoutsManager& cmdBufferLayouts = GetLayoutsManagerForCommandBuffer(cmdBuffer);

	const CommandBufferLayoutsManager::ImagesLayoutData& images = cmdBufferLayouts.GetAcquiredImagesLayouts();

	for (const auto& imageLayout : images)
	{
		const VkImage image = imageLayout.first;
		const ImageSubresourcesLayoutsData& layoutData = imageLayout.second;

		SPT_CHECK(layoutData.AreAllSubresourcesInSameLayout());

		SetFullImageLayout(cmdBuffer, image, layoutData.GetFullImageLayout());
	}
}

CommandBufferLayoutsManager& LayoutsManager::GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard cmdBuifferLayoutsManagersReadGuard(m_cmdBuffersLayoutManagersLock);

	CommandBufferLayoutsManager& cmdBufferLayouts = *m_cmdBuffersLayoutManagers[cmdBuffer].get();
	return cmdBufferLayouts;
}

const CommandBufferLayoutsManager& LayoutsManager::GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer) const
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard cmdBuifferLayoutsManagersReadGuard(m_cmdBuffersLayoutManagersLock);

	const auto& cmdBufferLayoutManager = m_cmdBuffersLayoutManagers.find(cmdBuffer);
	SPT_CHECK(cmdBufferLayoutManager != m_cmdBuffersLayoutManagers.cend());

	const CommandBufferLayoutsManager& cmdBufferLayouts = *cmdBufferLayoutManager->second;
	return cmdBufferLayouts;
}

VkImageLayout LayoutsManager::GetGlobalFullImageLayout(VkImage image) const
{
	SPT_PROFILE_FUNCTION();

	const lib::ReadLockGuard imageLayoutsReadLock(m_imageLayoutsLock);

	const auto foundLayout = m_imageLayouts.find(image);
	return foundLayout->second.m_fullImageLayout;
}

}