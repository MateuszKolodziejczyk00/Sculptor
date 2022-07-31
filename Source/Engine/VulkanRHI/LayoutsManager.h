#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

struct TextureSubresourceRange
{
	TextureSubresourceRange(Uint32 baseMipLevel = 0, Uint32 mipLevelsNum = 1, Uint32 baseArrayLayer = 0, Uint32 arrayLayersNum = 1)
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


class TextureLayoutCommandBufferData
{
public:

	TextureLayoutCommandBufferData(Uint32 mipsNum, Uint32 arrayLayers, VkImageLayout layout);

	Bool								AreAllSubresourcesInSameLayout() const;

	VkImageLayout						GetFullImageLayout() const;
	VkImageLayout						GetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer) const;
	VkImageLayout						GetSubresourcesSharedLayout(const TextureSubresourceRange& range) const;

	void								SetFullImageLayout(VkImageLayout layout);
	void								SetSubresourceLayout(Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void								SetSubresourcesLayout(const TextureSubresourceRange& range, VkImageLayout layout);

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


class TextureLayoutData
{
public:

	TextureLayoutData()
		: m_imageHandle(VK_NULL_HANDLE)
		, m_mipsNum(0)
		, m_arrayLayersNum(0)
		, m_fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	TextureLayoutData(VkImage imageHandle, Uint32 mipsNum, Uint32 arrayLayersNum)
		: m_imageHandle(imageHandle)
		, m_mipsNum(mipsNum)
		, m_arrayLayersNum(arrayLayersNum)
		, m_fullImageLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	{ }

	VkImage								m_imageHandle;

	Uint32								m_mipsNum;
	Uint32								m_arrayLayersNum;

	VkImageLayout						m_fullImageLayout;
};


class CommandBufferLayoutsManager
{
public:

	using TexturesLayoutData					= lib::HashMap<VkImage, TextureLayoutCommandBufferData>;

	CommandBufferLayoutsManager();

	void										AcquireTexture(const TextureLayoutData& texture);

	const TexturesLayoutData&					GetAcquiredTexturesLayouts() const;


private:

	using TexturesLayoutData					= lib::HashMap<VkImage, TextureLayoutCommandBufferData>;
	TexturesLayoutData							m_imageLayouts;
};


class LayoutsManager
{
public:

	LayoutsManager();

	void										RegisterTexture(VkImage image, Uint32 mipsNum, Uint32 arrayLayersNum);
	void										UnregisterTexture(VkImage image);

	VkImageLayout								GetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image) const;
	VkImageLayout								GetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer) const;
	VkImageLayout								GetSubresourcesSharedLayout(VkCommandBuffer cmdBuffer, VkImage image, const TextureSubresourceRange& range) const;

	void										SetFullImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout layout);
	void										SetSubresourceLayout(VkCommandBuffer cmdBuffer, VkImage image, Uint32 mipLevel, Uint32 arrayLayer, VkImageLayout layout);
	void										SetSubresourcesLayout(VkCommandBuffer cmdBuffer, VkImage image, const TextureSubresourceRange& range, VkImageLayout layout);

	void										RegisterCommandBuffer(VkCommandBuffer cmdBuffer);
	void										UnregisterCommnadBuffer(VkCommandBuffer cmdBuffer);

private:

	void										AcquireImage(VkCommandBuffer cmdBuffer, VkImage image);

	void										ReleaseCommandBufferResources(VkCommandBuffer cmdBuffer);

	CommandBufferLayoutsManager&				GetLayoutsManagerForCommandBuffer(VkCommandBuffer cmdBuffer);

	lib::HashMap<VkImage, TextureLayoutData>	m_imageLayouts;
	lib::ReadWriteLock							m_imageLayoutsLock;

	using CmdBufferToLayoutManager				= lib::HashMap<VkCommandBuffer, lib::UniquePtr<CommandBufferLayoutsManager>>;
	CmdBufferToLayoutManager					m_cmdBuffersLayoutManagers;
	lib::ReadWriteLock							m_cmdBuffersLayoutManagersLock;
};

}