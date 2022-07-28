#include "RendererUtils.h"
#include "Types/Semaphore.h"

namespace spt::renderer
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SemaphoresArray ===============================================================================

SemaphoresArray::SemaphoresArray()
{ }

void SemaphoresArray::AddBinarySemaphore(const lib::SharedPtr<Semaphore>& binarySemaphore, rhi::EPipelineStage::Flags submitStage)
{
	SPT_CHECK(binarySemaphore);

	m_rhiSemaphores.AddBinarySemaphore(binarySemaphore->GetRHI(), submitStage);
}

void SemaphoresArray::AddTimelineSemaphore(const lib::SharedPtr<Semaphore>& timelineSemaphore, rhi::EPipelineStage::Flags submitStage, Uint64 value)
{
	SPT_CHECK(timelineSemaphore);

	m_rhiSemaphores.AddTimelineSemaphore(timelineSemaphore->GetRHI(), submitStage, value);
}

const rhi::RHISemaphoresArray& SemaphoresArray::GetRHISemaphores() const
{
	return m_rhiSemaphores;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererUtils =================================================================================

Uint32 RendererUtils::GetFramesInFlightNum()
{
	return 3;
}

}
