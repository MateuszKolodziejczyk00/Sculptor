#include "Semaphore.h"
#include "RendererUtils.h"

namespace spt::rdr
{

Semaphore::Semaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	GetRHI().InitializeRHI(definition);
	GetRHI().SetName(name.Get());
}

}
