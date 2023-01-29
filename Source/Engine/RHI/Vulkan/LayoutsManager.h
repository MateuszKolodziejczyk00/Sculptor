#pragma once

#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"

namespace spt::rhi
{
struct TextureSubresourceRange;
}


namespace spt::vulkan
{

struct ImageSubresourceRange
{
	ImageSubresourceRange(Uint32 inBaseMipLevel = 0, Uint32 inMipLevelsNum = 1, Uint32 inBaseArrayLayer = 0, Uint32 inArrayLayersNum = 1)
		: baseMipLevel(inBaseMipLevel)
		, mipLevelsNum(inMipLevelsNum)
		, baseArrayLayer(inBaseArrayLayer)
		, arrayLayersNum(inArrayLayersNum)
	{ }

	ImageSubresourceRange(const VkImageSubresourceRange& subresourceRange)
		: baseMipLevel(subresourceRange.baseMipLevel)
		, mipLevelsNum(subresourceRange.levelCount)
		, baseArrayLayer(subresourceRange.baseArrayLayer)
		, arrayLayersNum(subresourceRange.layerCount)
	{ }

	Uint32		baseMipLevel;
	Uint32		mipLevelsNum;
	Uint32		baseArrayLayer;
	Uint32		arrayLayersNum;
};


class ImageSubresourcesLayoutsData
{
public:

	ImageSubresourcesLayoutsData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout);

	Bool								AreAllSubresourcesInSameLayout() const;

	VkImageLayout						GetFullImageLayout() const;
	VkImageLayout						GetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer) const;
	VkImageLayout						GetSubresourcesSharedLayout(const ImageSubresourceRange& range) const;

	void								SetFullImageLayout(VkImageLayout layout);
	void								SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void								SetSubresourcesLayout(const ImageSubresourceRange& range, VkImageLayout layout);

	Bool								HasWriteAccess() const;
	void								AcquireWriteAccess();
	void								ReleaseWriteAccess();

private:

	SizeType							GetSubresourceIdx(Uint32 mipLevel, Uint32 arrayLayer) const;

	void								TransitionToPerSubresourceLayout();

	Bool								TransitionToFullImageLayoutIfPossible();
	Bool								AreAllSubresourcesInSameLayoutImpl() const;

	Uint32								m_mipsNum;
	Uint32								m_arrayLayersNum;

	VkImageLayout						m_fullImageLayout;

	lib::DynamicArray<VkImageLayout>	m_subresourceLayouts;

	Bool								m_hasWriteAccess;
};


struct ImageLayoutData
{
public:

	ImageLayoutData()
		: mipsNum(0)
		, arrayLayersNum(0)
		, fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	ImageLayoutData(Uint32 inMipsNum, Uint32 inArrayLayersNum)
		: mipsNum(inMipsNum)
		, arrayLayersNum(inArrayLayersNum)
		, fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	Uint32								mipsNum;
	Uint32								arrayLayersNum;

	VkImageLayout						fullImageLayout;
};


class CommandBufferLayoutsManager
{
public:

	using ImagesLayoutData						= lib::HashMap<VkImage, ImageSubresourcesLayoutsData>;

	CommandBufferLayoutsManager();

	ImageSubresourcesLayoutsData*				AcquireImage(VkImage image, const ImageLayoutData& layoutInfo);

	const ImagesLayoutData&						GetAcquiredImagesLayouts() const;

	ImageSubresourcesLayoutsData*				GetImageLayoutInfo(VkImage image);
	const ImageSubresourcesLayoutsData*			GetImageLayoutInfo(VkImage image) const;

private:
	
	ImagesLayoutData							m_imageLayouts;
};


class LayoutsManager
{
public:

	LayoutsManager();

	void										RegisterImage(VkImage image, Uint32 mipsNum, Uint32 arrayLayersNum);
	void										UnregisterImage(VkImage image);

	VkImageLayout								GetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image) const;
	VkImageLayout								GetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer) const;
	VkImageLayout								GetSubresourcesSharedLayout(VkCommandBuffer cmdBuffer, VkImage image, const ImageSubresourceRange& range) const;
	VkImageLayout								GetSubresourcesSharedLayout(VkCommandBuffer cmdBuffer, VkImage image, const rhi::TextureSubresourceRange& range) const;

	void										SetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout layout);
	void										SetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void										SetSubresourcesLayout(VkCommandBuffer cmdBuffer, VkImage image, const ImageSubresourceRange& range, VkImageLayout layout);

	void										AcquireImageWriteAccess(VkCommandBuffer cmdBuffer, VkImage image);
	void										ReleaseImageWriteAccess(VkCommandBuffer cmdBuffer, VkImage image);

	void										RegisterRecordingCommandBuffer(VkCommandBuffer cmdBuffer);
	void										UnregisterRecordingCommnadBuffer(VkCommandBuffer cmdBuffer);

private:

	ImageSubresourcesLayoutsData&				GetAcquiredImageLayoutData(VkCommandBuffer cmdBuffer, VkImage image);
	const ImageSubresourcesLayoutsData*			GetImageLayoutDataIfAcquired(VkCommandBuffer cmdBuffer, VkImage image) const;

	ImageSubresourcesLayoutsData*				AcquireImage(CommandBufferLayoutsManager& cmdBufferLayoutsManager, VkImage image);
	void										ReleaseCommandBufferResources(VkCommandBuffer cmdBuffer);

	CommandBufferLayoutsManager&				GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer);
	const CommandBufferLayoutsManager&			GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer) const;

	void										SetGlobalFullImageLayout(VkImage image, VkImageLayout layout);
	VkImageLayout								GetGlobalFullImageLayout(VkImage image) const;

	lib::HashMap<VkImage, ImageLayoutData>		m_imageLayouts;
	mutable lib::ReadWriteLock					m_imageLayoutsLock;

	using CmdBufferToLayoutManager				= lib::HashMap<VkCommandBuffer, lib::UniquePtr<CommandBufferLayoutsManager>>;
	CmdBufferToLayoutManager					m_cmdBuffersLayoutManagers;
	mutable lib::ReadWriteLock					m_cmdBuffersLayoutManagersLock;
};

} // spt::vulkan