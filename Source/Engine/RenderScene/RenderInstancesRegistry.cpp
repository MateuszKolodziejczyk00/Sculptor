#include "RenderInstancesRegistry.h"
#include "RendererUtils.h"
#include "ResourcesManager.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderInstnacesList ===========================================================================

RenderInstnacesList::RenderInstnacesList(const rdr::RendererResourceName& listName, Uint32 maxInstancesNum)
	: m_lastAppendIdx(0)
{
	rhi::RHIAllocationInfo buffersAllocationInfo(rhi::EMemoryUsage::CPUToGPU);

	rhi::BufferDefinition instancesBufferDef;
	instancesBufferDef.size = static_cast<Uint64>(maxInstancesNum) * sizeof(Uint32);
	instancesBufferDef.usage = rhi::EBufferUsage::Storage;

	m_instances = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(listName + "_InstancesBuffer"), instancesBufferDef, buffersAllocationInfo);

	rhi::BufferDefinition instancesToRemoveBufferDef;
	instancesToRemoveBufferDef.size = static_cast<Uint64>(maxInstancesNum) * sizeof(Uint32);
	instancesToRemoveBufferDef.usage = rhi::EBufferUsage::Storage;

	m_instancesToRemove = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(listName + "_InstancesToRemoveBuffer"), instancesToRemoveBufferDef, buffersAllocationInfo);

	rhi::RHIAllocationInfo countBufferAllocationInfo(rhi::EMemoryUsage::CPUToGPU);

	rhi::BufferDefinition instancesToRemoveCountBufferDef;
	instancesToRemoveCountBufferDef.size = 4;
	instancesToRemoveCountBufferDef.usage = rhi::EBufferUsage::Storage;

	m_instancesToRemoveCount = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(listName + "_InstancesToRemoveCountBuffer"), instancesToRemoveCountBufferDef, buffersAllocationInfo);

	Byte* instancesBufferPtr = m_instances->GetRHI().MapBufferMemory();
	memset(instancesBufferPtr, 0, m_instances->GetRHI().GetSize());
	m_instances->GetRHI().UnmapBufferMemory();

	Uint32* instancesToRemoveCount = reinterpret_cast<Uint32*>(m_instancesToRemoveCount->GetRHI().MapBufferMemory());
	*instancesToRemoveCount = 0;
	m_instancesToRemoveCount->GetRHI().UnmapBufferMemory();
}

Uint32 RenderInstnacesList::AddInstance(Uint32 instanceDataOffset)
{
	SPT_PROFILER_FUNCTION();
	
	Uint32* instances = reinterpret_cast<Uint32*>(m_instances->GetRHI().MapBufferMemory());

	const Uint32 maxInstances = static_cast<Uint32>(m_instances->GetRHI().GetSize()) / sizeof(Uint32);

	const lib::LockGuard addLockGuard(m_addLock);

	Uint32 currentIdx = m_lastAppendIdx + 1;

	Uint32 instanceIdx = idxNone<Uint32>;

	while (currentIdx != m_lastAppendIdx)
	{
		if (instances[currentIdx] == 0)
		{
			instanceIdx = currentIdx;
			break;
		}

		if (++currentIdx >= maxInstances)
		{
			currentIdx = 0;
		}
	}

	m_instances->GetRHI().UnmapBufferMemory();

	return instanceIdx;
}

void RenderInstnacesList::RemoveInstance(Uint32 idx)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO: InstancesToRemove buffer needs double buffering");
}

const lib::SharedPtr<rdr::Buffer>& RenderInstnacesList::GetInstancesBuffer() const
{
	return m_instances;
}

const lib::SharedPtr<rdr::Buffer>& RenderInstnacesList::GetInstancesToRemoveBuffer() const
{
	return m_instancesToRemove;
}

const lib::SharedPtr<rdr::Buffer>& RenderInstnacesList::GetInstancesToRemoveCountBuffer() const
{
	return m_instancesToRemoveCount;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderInstancesRegistry =======================================================================

RenderInstancesRegistry::RenderInstancesRegistry()
	: m_basePassInstances(RENDERER_RESOURCE_NAME("BasePassInstancesList"), 1024)
{ }

RenderInstnacesList& RenderInstancesRegistry::GetBasePassInstances()
{
	return m_basePassInstances;
}

} // spt::rsc
