#include "RendererUtils.h"
#include "Types/Semaphore.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SemaphoresArray ===============================================================================

SemaphoresArray::SemaphoresArray()
{ }

void SemaphoresArray::AddBinarySemaphore(const lib::SharedPtr<Semaphore>& binarySemaphore, rhi::EPipelineStage submitStage)
{
	SPT_CHECK(binarySemaphore);

	m_rhiSemaphores.AddBinarySemaphore(binarySemaphore->GetRHI(), submitStage);
}

void SemaphoresArray::AddTimelineSemaphore(const lib::SharedPtr<Semaphore>& timelineSemaphore, Uint64 value, rhi::EPipelineStage submitStage)
{
	SPT_CHECK(timelineSemaphore);

	m_rhiSemaphores.AddTimelineSemaphore(timelineSemaphore->GetRHI(), value, submitStage);
}

const rhi::RHISemaphoresArray& SemaphoresArray::GetRHISemaphores() const
{
	return m_rhiSemaphores;
}

void SemaphoresArray::Reset()
{
	m_rhiSemaphores.Reset();
}

void SemaphoresArray::Append(const SemaphoresArray& other)
{
	m_rhiSemaphores.Append(other.m_rhiSemaphores);
}

} // spt::rdr
