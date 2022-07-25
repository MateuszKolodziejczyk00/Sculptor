#pragma once

#include "RendererTypesMacros.h"
#include "RHISemaphoreImpl.h"


namespace spt::rhi
{
struct SemaphoreDefinition;
}


namespace spt::renderer
{

struct RendererResourceName;


class RENDERER_TYPES_API Semaphore
{
public:

	Semaphore(const RendererResourceName& name, const rhi::SemaphoreDefinition& definition);
	~Semaphore();

	rhi::RHISemaphore&				GetRHI();
	const rhi::RHISemaphore&		GetRHI() const;

private:
	
	rhi::RHISemaphore				m_rhiSemaphore;
};

}