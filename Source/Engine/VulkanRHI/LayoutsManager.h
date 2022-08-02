#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

struct ImageSubresourceRange
{
	ImageSubresourceRange(Uint32 baseMipLevel = 0, Uint32 mipLevelsNum = 1, Uint32 baseArrayLayer = 0, Uint32 arrayLayersNum = 1)
		: m_baseMipLevel(baseMipLevel)
		, m_mipLevelsNum(mipLevelsNum)
		, m_baseArrayLayer(baseArrayLayer)
		, m_arrayLayersNum(arrayLayersNum)
	{ }

	Uint32		m_baseMipLevel;
	Uint32		m_mipLevelsNum;
	Uint32		m_baseArrayLayer;
	Uint32		m_arrayLayersNum;
};


class ImageLayoutCommandBufferData
{
public:

	ImageLayoutCommandBufferData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout);

	Bool								AreAllSubresourcesInSameLayout() const;

	VkImageLayout						GetFullImageLayout() const;
	VkImageLayout						GetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer) const;
	VkImageLayout						GetSubresourcesSharedLayout(const ImageSubresourceRange& range) const;

	void								SetFullImageLayout(VkImageLayout layout);
	void								SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void								SetSubresourcesLayout(const ImageSubresourceRange& range, VkImageLayout layout);

private:

	SizeType							GetSubresourceIdx(Uint32 mipLevel, Uint32 arrayLayer) const;

	void								TransitionToPerSubresourceLayout();

	Bool								TransitionToFullImageLayoutIfPossible();
	Bool								AreAllSubresourcesInSameLayoutImpl() const;

	Uint32								m_mipsNum;
	Uint32								m_arrayLayersNum;

	VkImageLayout						m_fullImageLayout;

	lib::DynamicArray<VkImageLayout>	m_subresourceLayouts;
};


class ImageLayoutData
{
public:

	ImageLayoutData()
		: m_mipsNum(0)
		, m_arrayLayersNum(0)
		, m_fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	ImageLayoutData(Uint32 mipsNum, Uint32 arrayLayersNum)
		: m_mipsNum(mipsNum)
		, m_arrayLayersNum(arrayLayersNum)
		, m_fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	Uint32								m_mipsNum;
	Uint32								m_arrayLayersNum;

	VkImageLayout						m_fullImageLayout;
};


class CommandBufferLayoutsManager
{
public:

	using ImagesLayoutData					= lib::HashMap<VkImage, ImageLayoutCommandBufferData>;

	CommandBufferLayoutsManager();

	ImageLayoutCommandBufferData*				AcquireImage(VkImage image, const ImageLayoutData& layoutInfo);

	const ImagesLayoutData&						GetAcquiredImagesLayouts() const;

	ImageLayoutCommandBufferData*				GetImageLayoutInfo(VkImage image);
	const ImageLayoutCommandBufferData*			GetImageLayoutInfo(VkImage image) const;

private:

	using ImagesLayoutData						= lib::HashMap<VkImage, ImageLayoutCommandBufferData>;
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

	void										SetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout layout);
	void										SetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void										SetSubresourcesLayout(VkCommandBuffer cmdBuffer, VkImage image, const ImageSubresourceRange& range, VkImageLayout layout);

	void										RegisterCommandBuffer(VkCommandBuffer cmdBuffer);
	void										UnregisterCommnadBuffer(VkCommandBuffer cmdBuffer);

private:

	ImageLayoutCommandBufferData&				GetAcquiredImageLayoutData(VkCommandBuffer cmdBuffer, VkImage image);
	const ImageLayoutCommandBufferData*			GetImageLayoutDataIfAcquired(VkCommandBuffer cmdBuffer, VkImage image) const;

	ImageLayoutCommandBufferData*				AcquireImage(CommandBufferLayoutsManager& cmdBufferLayoutsManager, VkImage image);
	void										ReleaseCommandBufferResources(VkCommandBuffer cmdBuffer);

	CommandBufferLayoutsManager&				GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer);
	const CommandBufferLayoutsManager&			GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer) const;

	VkImageLayout								GetGlobalFullImageLayout(VkImage image) const;

	lib::HashMap<VkImage, ImageLayoutData>		m_imageLayouts;
	mutable lib::ReadWriteLock					m_imageLayoutsLock;

	using CmdBufferToLayoutManager				= lib::HashMap<VkCommandBuffer, lib::UniquePtr<CommandBufferLayoutsManager>>;
	CmdBufferToLayoutManager					m_cmdBuffersLayoutManagers;
	mutable lib::ReadWriteLock					m_cmdBuffersLayoutManagersLock;
};

}