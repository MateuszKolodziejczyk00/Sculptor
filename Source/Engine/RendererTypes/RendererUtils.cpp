#include "RendererUtils.h"
#include "Types/Semaphore.h"

namespace spt::renderer
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererUtils =================================================================================

Uint32 RendererUtils::GetFramesInFlightNum()
{
	return 3;
}

}
