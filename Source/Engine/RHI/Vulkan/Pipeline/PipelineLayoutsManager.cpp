#include "PipelineLayoutsManager.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"

namespace spt::vulkan
{

void PipelineLayoutsManager::InitializeRHI()
{
	// nothing here for now
}

void PipelineLayoutsManager::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	m_pipelinesPendingFlush.clear();
	m_cachedPipelineLayouts.clear();
}

lib::SharedRef<PipelineLayout> PipelineLayoutsManager::GetOrCreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	const SizeType layoutDefinitionHash = lib::GetHash(definition);

	const auto cachedPipelineLayoutIt = m_cachedPipelineLayouts.find(layoutDefinitionHash);
	if (cachedPipelineLayoutIt != std::cend(m_cachedPipelineLayouts))
	{
		return lib::Ref(cachedPipelineLayoutIt->second);
	}

	{
		const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);

		auto pendingPipelineLayoutIt = m_pipelinesPendingFlush.find(layoutDefinitionHash);
		if (pendingPipelineLayoutIt == std::cend(m_pipelinesPendingFlush))
		{
			pendingPipelineLayoutIt = m_pipelinesPendingFlush.emplace(layoutDefinitionHash, CreatePipelineLayout(definition)).first;
		}

		return lib::Ref(pendingPipelineLayoutIt->second);
	}
}

void PipelineLayoutsManager::FlushPendingPipelineLayouts()
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lockGuard(m_pipelinesPendingFlushLock);

	std::move(std::cbegin(m_pipelinesPendingFlush), std::cend(m_pipelinesPendingFlush), std::inserter(m_cachedPipelineLayouts, std::end(m_cachedPipelineLayouts)));
	m_pipelinesPendingFlush.clear();
}

lib::SharedRef<PipelineLayout> PipelineLayoutsManager::CreatePipelineLayout(const rhi::PipelineLayoutDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	return lib::MakeShared<PipelineLayout>(definition);
}

} // spt::vulkan
