#pragma once

#include "SculptorCoreTypes.h"


namespace spt::renderer
{

struct RendererResourceName
{
public:

#if RENDERER_VALIDATION

	RendererResourceName(const lib::HashedString& name)
		: m_name(name)
	{ }

#else

	RendererResourceName() {}

#endif // RENDERER_VALIDATION

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

	lib::HashedString m_name;
};

#if RENDERER_VALIDATION

#define RENDERER_RESOURCE_NAME(Name) RendererResourceName(Name)

#else

#define RENDERER_RESOURCE_NAME(Name) RendererResourceName()

#endif // RENDERER_VALIDATION

}