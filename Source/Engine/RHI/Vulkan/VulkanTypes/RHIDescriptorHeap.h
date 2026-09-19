#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "SculptorCoreTypes.h"
#include "RHIBuffer.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "RHICore/RHISamplerTypes.h"


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

	rhi::EDescriptorHeapType GetType() const;

	rhi::RHIDescriptorRange AllocateRange(Uint64 size);
	void DeallocateRange(rhi::RHIDescriptorRange range);

	Uint32 GetDescriptorSize() const { return m_descriptorSize; }
	Uint32 GetDescriptorsNum() const { return m_descriptorsNum; }

	lib::Span<Byte> GetDescriptorData(Uint32 idx) const
	{
		SPT_CHECK(idx < m_descriptorsNum);
		return m_mappedBuffer.Get().GetSpan().subspan(idx * m_descriptorSize, m_descriptorSize);
	}

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	static void CopySamplerDescriptor(const rhi::SamplerDefinition& def, lib::Span<Byte> descriptorData);

	// Vulkan ================================================

	const RHIBuffer& GetBuffer() const { return m_buffer; }

private:

	RHIBuffer m_buffer;
	lib::Lock m_lock;

	Uint32 m_descriptorSize = 0u;
	Uint32 m_descriptorsNum = 0u;

	lib::TypeStorage<RHIMappedByteBuffer> m_mappedBuffer;
};

} // spt::vulkan
