#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHISemaphoreImpl.h"


namespace spt::renderer
{

class Semaphore;

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererResourceName ==========================================================================

struct RENDERER_TYPES_API RendererResourceName
{
public:

#if RENDERER_VALIDATION

	RendererResourceName(const lib::HashedString& name)
		: m_name(name)
	{ }

#endif // RENDERER_VALIDATION

	RendererResourceName() {}


	const lib::HashedString& Get() const
	{
#if RENDERER_VALIDATION
		return m_name;
#else
		static const lib::HashedString dummyName{};
		return dummyName;
#endif // RENDERER_VALIDATION
	}


private:

#if RENDERER_VALIDATION

	lib::HashedString m_name;

#endif // RENDERER_VALIDATION
};

#if RENDERER_VALIDATION

#define RENDERER_RESOURCE_NAME(Name) spt::renderer::RendererResourceName(Name)

#else

#define RENDERER_RESOURCE_NAME(Name) spt::renderer::RendererResourceName()

#endif // RENDERER_VALIDATION

//////////////////////////////////////////////////////////////////////////////////////////////////
// SemaphoresArray ===============================================================================

class RENDERER_TYPES_API SemaphoresArray
{
public:

	SemaphoresArray();

	void								AddBinarySemaphore(const lib::SharedPtr<Semaphore>& binarySemaphore, rhi::EPipelineStage::Flags submitStage);
	void								AddTimelineSemaphore(const lib::SharedPtr<Semaphore>& timelineSemaphore, Uint64 value, rhi::EPipelineStage::Flags submitStage);

	const rhi::RHISemaphoresArray&		GetRHISemaphores() const;

private:

	rhi::RHISemaphoresArray				m_rhiSemaphores;

};

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererUtils =================================================================================

class RENDERER_TYPES_API RendererUtils
{
public:

	static Uint32			GetFramesInFlightNum();
};

}