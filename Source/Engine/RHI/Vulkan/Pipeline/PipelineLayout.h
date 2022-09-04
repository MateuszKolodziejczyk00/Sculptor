#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineLayoutTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class PipelineLayout
{
public:

	PipelineLayout();

	void				InitializeRHI(const rhi::PipelineLayoutDefinition& definition);
	void				ReleaseRHI();

	Bool				IsValid() const;

	VkPipelineLayout	GetHandle() const;

private:

	VkPipelineLayout	m_layoutHnadle;
};

} // spt::vulkan
