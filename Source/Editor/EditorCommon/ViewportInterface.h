#pragma once

#include "EditorCommonMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::ed
{

class EDITOR_COMMON_API ViewportInterface
{
public:

	virtual ~ViewportInterface() = default;

	virtual Bool IsFocused() const = 0;
};

} // spt::ed
