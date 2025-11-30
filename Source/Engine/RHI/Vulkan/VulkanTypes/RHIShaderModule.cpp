#include "RHIShaderModule.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIShaderModuleReleaseTicket ==================================================================

void RHIShaderModuleReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyShaderModule(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIShaderModule ===============================================================================

RHIShaderModule::RHIShaderModule()
	: m_handle(VK_NULL_HANDLE)
	, m_stage(rhi::EShaderStage::Vertex)
{ }

void RHIShaderModule::InitializeRHI(const rhi::ShaderModuleDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize	= definition.binary.size();
    moduleInfo.pCode	= reinterpret_cast<const Uint32*>(definition.binary.data());
	m_stage				= definition.stage;
	m_entryPoint		= definition.entryPoint;

	SPT_VK_CHECK(vkCreateShaderModule(VulkanRHI::GetDeviceHandle(), &moduleInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle))
}

void RHIShaderModule::ReleaseRHI()
{
	RHIShaderModuleReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIShaderModuleReleaseTicket RHIShaderModule::DeferredReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	RHIShaderModuleReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif SPT_RHI_DEBUG

	m_name.Reset(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_SHADER_MODULE);
	
	m_stage = rhi::EShaderStage::None;
	m_entryPoint.Reset();

	m_handle = VK_NULL_HANDLE;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

Bool RHIShaderModule::IsValid() const
{
	return GetHandle() != VK_NULL_HANDLE;
}

rhi::EShaderStage RHIShaderModule::GetStage() const
{
	return m_stage;
}

const lib::HashedString& RHIShaderModule::GetEntryPoint() const
{
	return m_entryPoint;
}

void RHIShaderModule::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_SHADER_MODULE);
}

const lib::HashedString& RHIShaderModule::GetName() const
{
	return m_name.Get();
}

VkShaderModule RHIShaderModule::GetHandle() const
{
	return m_handle;
}

} // spt::vulkan
