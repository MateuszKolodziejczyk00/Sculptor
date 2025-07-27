#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "SculptorCoreTypes.h"
#include "RHIBuffer.h"
#include "RHICore/RHIDescriptorTypes.h"


namespace spt::vulkan
{

struct RHI_API RHIDescriptorHeapReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIBufferReleaseTicket bufferHandle;
};


class RHI_API RHIDescriptorHeap
{
public:

	RHIDescriptorHeap();

	void InitializeRHI(const rhi::DescriptorHeapDefinition& definition);
	void ReleaseRHI();

	RHIDescriptorHeapReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	rhi::RHIDescriptorRange AllocateRange(Uint64 size);
	void DeallocateRange(rhi::RHIDescriptorRange range);

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan ================================================

	const RHIBuffer& GetBuffer() const { return m_buffer; }

private:

	RHIBuffer m_buffer;
	lib::Lock m_lock;
};

} // spt::vulkan
