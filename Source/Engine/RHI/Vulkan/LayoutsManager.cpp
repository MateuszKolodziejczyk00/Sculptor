#include "LayoutsManager.h"
#include "RHICore/RHITextureTypes.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// ImageSubresourcesLayoutsData ==================================================================

ImageSubresourcesLayoutsData::ImageSubresourcesLayoutsData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout)
	: m_mipsNum(mipsNum)
	, m_arrayLayersNum(arrayLayers)
	, m_fullImageLayout(layout)
	, m_hasWriteAccess(true)
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
	SPT_PROFILER_FUNCTION();

	if (AreAllSubresourcesInSameLayout())
	{
		return GetFullImageLayout();
	}

	const Uint32 endMipLevel = range.baseMipLevel + range.mipLevelsNum;
	const Uint32 endArrayLayer = range.baseArrayLayer + range.arrayLayersNum;

	const VkImageLayout layout = m_subresourceLayouts[GetSubresourceIdx(range.baseMipLevel, range.baseArrayLayer)];

#if DO_CHECKS
	for (Uint32 mipLevel = range.baseMipLevel; mipLevel < endMipLevel; ++mipLevel)
	{
		for (Uint32 arrayLayer = range.baseArrayLayer; arrayLayer < endArrayLayer; ++arrayLayer)
		{
			SPT_CHECK(m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)] == layout);
		}
	}
#endif // TO_CHECK

	return layout;
}

void ImageSubresourcesLayoutsData::SetFullImageLayout(VkImageLayout layout)
{
	SPT_CHECK_MSG(HasWriteAccess(), "Attempt to change layout without write access!");

	m_fullImageLayout = layout;
	m_subresourceLayouts.clear();
}

void ImageSubresourcesLayoutsData::SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout)
{
	SPT_CHECK_MSG(HasWriteAccess(), "Attempt to change layout without write access!");

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
		m_fullImageLayout = layout;
	}
}

void ImageSubresourcesLayoutsData::SetSubresourcesLayout(const ImageSubresourceRange& range, VkImageLayout layout)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_MSG(HasWriteAccess(), "Attempt to change layout without write access!");

	if (	range.baseMipLevel == 0 && range.baseArrayLayer == 0
		&& (range.mipLevelsNum == m_mipsNum || range.mipLevelsNum == rhi::constants::allRemainingMips)
		&& (range.arrayLayersNum == m_arrayLayersNum || range.arrayLayersNum == rhi::constants::allRemainingArrayLayers))
	{
		SetFullImageLayout(layout);
	}
	else if (range.mipLevelsNum == 1 && range.arrayLayersNum == 1)
	{
		SetSubresourceLayout(range.baseMipLevel, range.baseArrayLayer, layout);
	}
	else
	{
		if (!AreAllSubresourcesInSameLayout())
		{
			TransitionToPerSubresourceLayout();
		}

		const Uint32 endMipLevel = range.baseMipLevel + range.mipLevelsNum;
		const Uint32 endArrayLayer = range.baseArrayLayer + range.arrayLayersNum;

		for (Uint32 mipLevel = range.baseMipLevel; mipLevel < endMipLevel; ++mipLevel)
		{
			for (Uint32 arrayLayer = range.baseArrayLayer; arrayLayer < endArrayLayer; ++arrayLayer)
			{
				m_subresourceLayouts[GetSubresourceIdx(mipLevel, arrayLayer)] = layout;
			}
		}

		TransitionToFullImageLayoutIfPossible();
	}
}

Bool ImageSubresourcesLayoutsData::HasWriteAccess() const
{
	return m_hasWriteAccess;
}

void ImageSubresourcesLayoutsData::AcquireWriteAccess()
{
	m_hasWriteAccess = true;
}

void ImageSubresourcesLayoutsData::ReleaseWriteAccess()
{
	m_hasWriteAccess = false;
}

SizeType ImageSubresourcesLayoutsData::GetSubresourceIdx(Uint32 mipLevel, Uint32 arrayLayer) const
{
	return static_cast<SizeType>(arrayLayer + mipLevel * m_arrayLayersNum);
}

void ImageSubresourcesLayoutsData::TransitionToPerSubresourceLayout()
{
	SPT_PROFILER_FUNCTION();

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
		SetFullImageLayout(m_subresourceLayouts[0]);
		return true;
	}

	return false;
}

Bool ImageSubresourcesLayoutsData::AreAllSubresourcesInSameLayoutImpl() const
{
	SPT_PROFILER_FUNCTION();

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
	const ImageSubresourcesLayoutsData acquiredImageLayoutData(layoutInfo.mipsNum, layoutInfo.arrayLayersNum, layoutInfo.fullImageLayout);

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
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard imageLayoutsWriteLock(m_imageLayoutsLock);

	m_imageLayouts.emplace(image, ImageLayoutData(mipsNum, arrayLayersNum));
}

void LayoutsManager::UnregisterImage(VkImage image)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard imageLayoutsWriteLock(m_imageLayoutsLock);

	m_imageLayouts.erase(image);
}

VkImageLayout LayoutsManager::GetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image) const
{
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();

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

VkImageLayout LayoutsManager::GetSubresourcesSharedLayout(VkCommandBuffer cmdBuffer, VkImage image, const rhi::TextureSubresourceRange& range) const
{
	ImageSubresourceRange imageRange;
	imageRange.baseMipLevel		= range.baseMipLevel;
	imageRange.mipLevelsNum		= range.mipLevelsNum;
	imageRange.baseArrayLayer	= range.baseArrayLayer;
	imageRange.arrayLayersNum	= range.arrayLayersNum;

	return GetSubresourcesSharedLayout(cmdBuffer, image, imageRange);
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

void LayoutsManager::AcquireImageWriteAccess(VkCommandBuffer cmdBuffer, VkImage image)
{
	ImageSubresourcesLayoutsData& layoutData = GetAcquiredImageLayoutData(cmdBuffer, image);
	layoutData.AcquireWriteAccess();
}

void LayoutsManager::ReleaseImageWriteAccess(VkCommandBuffer cmdBuffer, VkImage image)
{
	ImageSubresourcesLayoutsData& layoutData = GetAcquiredImageLayoutData(cmdBuffer, image);

	SPT_CHECK_MSG(layoutData.GetFullImageLayout() == GetGlobalFullImageLayout(image), "Cannot release image with changed layout");

	layoutData.ReleaseWriteAccess();
}

void LayoutsManager::RegisterRecordingCommandBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	const lib::WriteLockGuard cmdBuifferLayoutsManagersWriteGuard(m_cmdBuffersLayoutManagersLock);

	m_cmdBuffersLayoutManagers.emplace(cmdBuffer, std::make_unique<CommandBufferLayoutsManager>());
}

void LayoutsManager::UnregisterRecordingCommnadBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

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
	SPT_PROFILER_FUNCTION();

	ImageLayoutData imageLayoutData;

	{
		const lib::ReadLockGuard imageLayoutsReadGuard(m_imageLayoutsLock);

		imageLayoutData = m_imageLayouts[image];
	}

	return cmdBufferLayoutsManager.AcquireImage(image, imageLayoutData);
}

void LayoutsManager::ReleaseCommandBufferResources(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	const CommandBufferLayoutsManager& cmdBufferLayouts = GetLayoutsManagerForCommandBuffer(cmdBuffer);

	const CommandBufferLayoutsManager::ImagesLayoutData& images = cmdBufferLayouts.GetAcquiredImagesLayouts();

	for (const auto& [image, layoutData] : images)
	{
		if (layoutData.HasWriteAccess())
		{
			SPT_CHECK(layoutData.AreAllSubresourcesInSameLayout());

			SetFullImageLayout(cmdBuffer, image, layoutData.GetFullImageLayout());
		}
	}
}

CommandBufferLayoutsManager& LayoutsManager::GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	const lib::ReadLockGuard cmdBuifferLayoutsManagersReadGuard(m_cmdBuffersLayoutManagersLock);

	const auto cmdBufferLayoutManager = m_cmdBuffersLayoutManagers.find(cmdBuffer);
	SPT_CHECK(cmdBufferLayoutManager != m_cmdBuffersLayoutManagers.cend());

	CommandBufferLayoutsManager& cmdBufferLayouts = *cmdBufferLayoutManager->second;
	return cmdBufferLayouts;
}

const CommandBufferLayoutsManager& LayoutsManager::GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer) const
{
	SPT_PROFILER_FUNCTION();

	const lib::ReadLockGuard cmdBuifferLayoutsManagersReadGuard(m_cmdBuffersLayoutManagersLock);

	const auto cmdBufferLayoutManager = m_cmdBuffersLayoutManagers.find(cmdBuffer);
	SPT_CHECK(cmdBufferLayoutManager != m_cmdBuffersLayoutManagers.cend());

	const CommandBufferLayoutsManager& cmdBufferLayouts = *cmdBufferLayoutManager->second;
	return cmdBufferLayouts;
}

VkImageLayout LayoutsManager::GetGlobalFullImageLayout(VkImage image) const
{
	SPT_PROFILER_FUNCTION();

	const lib::ReadLockGuard imageLayoutsReadLock(m_imageLayoutsLock);

	const auto foundLayout = m_imageLayouts.find(image);
	SPT_CHECK(foundLayout != m_imageLayouts.cend());

	return foundLayout->second.fullImageLayout;
}

} // spt::vulkan