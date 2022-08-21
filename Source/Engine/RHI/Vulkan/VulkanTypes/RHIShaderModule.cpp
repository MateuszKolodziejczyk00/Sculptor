#include "RHIShaderModule.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

RHIShaderModule::RHIShaderModule()
	: m_handle(VK_NULL_HANDLE)
	, m_stage(rhi::EShaderStage::Vertex)
{ }

void RHIShaderModule::InitializeRHI(const rhi::ShaderModuleDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize = static_cast<Uint32>(definition.binary.size() * sizeof(Uint32));
    moduleInfo.pCode = definition.binary.data();

	SPT_VK_CHECK(vkCreateShaderModule(VulkanRHI::GetDeviceHandle(), &moduleInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle))
}

void RHIShaderModule::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	vkDestroyShaderModule(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());

	m_handle = VK_NULL_HANDLE;
}

Bool RHIShaderModule::IsValid() const
{
	return GetHandle() != VK_NULL_HANDLE;
}

rhi::EShaderStage RHIShaderModule::GetStage() const
{
	return m_stage;
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

}
