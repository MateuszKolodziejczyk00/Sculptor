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

	Uint64 GetHeapSize() const;

	lib::Span<Byte> GetBufferDescriptorData(Uint32 idx) const;
	lib::Span<Byte> GetTextureDescriptorData(Uint32 idx) const;
	lib::Span<Byte> GetSamplerDescriptorData(Uint32 idx) const;

	Uint32 GetBufferDescriptorsNum() const;
	Uint32 GetTextureDescriptorsNum() const;
	Uint32 GetSamplerDescriptorsNum() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	static void CopySamplerDescriptor(const rhi::SamplerDefinition& def, lib::Span<Byte> descriptorData);

	// Vulkan ================================================

	const RHIBuffer& GetBuffer() const { return m_buffer; }

private:

	RHIBuffer m_buffer;

	lib::TypeStorage<RHIMappedByteBuffer> m_mappedBuffer;
};

} // spt::vulkan
