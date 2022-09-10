#include "RHIDescriptorSet.h"

namespace spt::vulkan
{

RHIDescriptorSet::RHIDescriptorSet()
	: m_handle(VK_NULL_HANDLE)
	, m_poolSetIdx(idxNone<Uint16>)
	, m_poolIdx(idxNone<Uint16>)
{ }

RHIDescriptorSet::RHIDescriptorSet(VkDescriptorSet handle, Uint16 poolSetIdx, Uint16 poolIdx)
	: m_handle(handle)
	, m_poolSetIdx(poolSetIdx)
	, m_poolIdx(poolIdx)
{
	SPT_CHECK(m_handle != VK_NULL_HANDLE);
	SPT_CHECK(m_poolSetIdx != idxNone<Uint16>);
	SPT_CHECK(m_poolIdx != idxNone<Uint16>);
}

Bool RHIDescriptorSet::IsValid() const
{
	return m_handle != VK_NULL_HANDLE && m_poolSetIdx != idxNone<Uint16> && m_poolIdx != idxNone<Uint16>;
}

VkDescriptorSet RHIDescriptorSet::GetHandle() const
{
	return m_handle;
}

Uint16 RHIDescriptorSet::GetPoolSetIdx() const
{
	return m_poolSetIdx;
}

Uint16 RHIDescriptorSet::GetPoolIdx() const
{
	return m_poolIdx;
}

} // spt::vulkan
