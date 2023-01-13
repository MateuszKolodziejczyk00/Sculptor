#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIShaderTypes.h"


namespace spt::vulkan
{

class RHI_API RHIShaderModule
{
public:


	RHIShaderModule();

	void						InitializeRHI(const rhi::ShaderModuleDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	rhi::EShaderStage			GetStage() const;
	const lib::HashedString&	GetEntryPoint() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ================================================

	VkShaderModule				GetHandle() const;

private:

	VkShaderModule				m_handle;

	rhi::EShaderStage			m_stage;
	lib::HashedString			m_entryPoint;

	DebugName					m_name;
};

}