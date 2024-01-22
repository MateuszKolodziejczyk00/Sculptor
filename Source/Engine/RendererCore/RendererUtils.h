#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHISemaphoreImpl.h"


namespace spt::rdr
{

class Semaphore;

//////////////////////////////////////////////////////////////////////////////////////////////////
// RendererResourceName ==========================================================================

struct RENDERER_CORE_API RendererResourceName
{
public:

#if RENDERER_VALIDATION

	explicit RendererResourceName(const lib::HashedString& name)
		: m_name(name)
	{ }

#else

	explicit RendererResourceName(const lib::HashedString& name)
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

	RendererResourceName operator+(const char* suffix) const
	{
#if RENDERER_VALIDATION
		return RendererResourceName(Get().ToString() + suffix);
#else
		return RendererResourceName();
#endif // RENDERER_VALIDATION
	}

private:

#if RENDERER_VALIDATION

	lib::HashedString m_name;

#endif // RENDERER_VALIDATION
};

#if RENDERER_VALIDATION

#define RENDERER_RESOURCE_NAME(Name) spt::rdr::RendererResourceName(Name)
#define RENDERER_RESOURCE_NAME_FORMATTED(...) spt::rdr::RendererResourceName(std::format(__VA_ARGS__))

#else

#define RENDERER_RESOURCE_NAME(Name) spt::rdr::RendererResourceName()
#define RENDERER_RESOURCE_NAME_FORMATTED(...) spt::rdr::RendererResourceName()

#endif // RENDERER_VALIDATION

//////////////////////////////////////////////////////////////////////////////////////////////////
// SemaphoresArray ===============================================================================

class RENDERER_CORE_API SemaphoresArray
{
public:

	SemaphoresArray();

	void								AddBinarySemaphore(const lib::SharedPtr<Semaphore>& binarySemaphore, rhi::EPipelineStage submitStage);
	void								AddTimelineSemaphore(const lib::SharedPtr<Semaphore>& timelineSemaphore, Uint64 value, rhi::EPipelineStage submitStage);

	const rhi::RHISemaphoresArray&		GetRHISemaphores() const;

	void								Reset();

	void								Append(const SemaphoresArray& other);

private:

	rhi::RHISemaphoresArray				m_rhiSemaphores;

};

} // spt::rdr