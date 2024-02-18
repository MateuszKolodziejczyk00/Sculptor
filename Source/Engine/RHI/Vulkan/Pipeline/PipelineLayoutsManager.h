#pragma once

#include "SculptorCoreTypes.h"
#include "Vulkan/VulkanCore.h"
#include "PipelineLayout.h"


namespace spt::vulkan
{

class PipelineLayoutsManager
{
public:

	PipelineLayoutsManager() = default;

	void							InitializeRHI();
	void							ReleaseRHI();

	lib::SharedRef<PipelineLayout>	GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

	void							FlushPendingPipelineLayouts();

private:


	lib::SharedRef<PipelineLayout>	CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition);

	/**
	 * Cached handles that were flushed.
	 * Access to these object can be concurrent during rendering
	 */
	lib::HashMap<SizeType, lib::SharedPtr<PipelineLayout>>	m_cachedPipelineLayouts;

	/**
	 * Handles created after last flush.
	 * Access to these objects is slower, as it has to be synchronized using lock (because some thread may modify its content by creating new layout)
	 */
	lib::HashMap<SizeType, lib::SharedPtr<PipelineLayout>>	m_pipelinesPendingFlush;
	lib::Lock												m_pipelinesPendingFlushLock;
};

} // spt::vulkan
