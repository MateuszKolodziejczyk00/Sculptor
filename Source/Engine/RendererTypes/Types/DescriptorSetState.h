#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rdr
{

using DSStateID = Uint64;

/**
 * Base class for all descriptor set states
 * Doesn't provide any virtual functions, as it should be always using as template argument
 * Doesn't provide virtual destructor, because it's handled by shared pointer's deleter
 */
class RENDERER_TYPES_API DescriptorSetState
{
public:

	DescriptorSetState();

	DSStateID	GetID() const;

	Bool		IsDirty() const;
	void		ClearDirtyFlag();

protected:

	void MarkAsDirty();

private:

	const DSStateID	m_state;
	Bool			m_isDirty;
};

} // spt::rdr
