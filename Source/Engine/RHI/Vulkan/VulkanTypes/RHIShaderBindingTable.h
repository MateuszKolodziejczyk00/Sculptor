#pragma once

#include "RHIMacros.h"
#include "RHICore/RHIPipelineDefinitionTypes.h"
#include "RHIBuffer.h"


namespace spt::vulkan
{

class RHIPipeline;


class RHI_API RHIShaderBindingTable
{
public:

	RHIShaderBindingTable();

	void InitializeRHI(const RHIPipeline& pipeline, const rhi::RayTracingShadersDefinition& shadersDef);
	void ReleaseRHI();

	const VkStridedDeviceAddressRegionKHR& GetRayGenRegion() const;
	const VkStridedDeviceAddressRegionKHR& GetClosestHitRegion() const;
	const VkStridedDeviceAddressRegionKHR& GetMissRegion() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

private:

	RHIBuffer m_sbtBuffer;

	VkStridedDeviceAddressRegionKHR m_rayGenRegion;
	VkStridedDeviceAddressRegionKHR m_closestHitRegion;
	VkStridedDeviceAddressRegionKHR m_missRegion;
};

} // spt::vulkan