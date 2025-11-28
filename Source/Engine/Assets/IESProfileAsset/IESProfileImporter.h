#pragma once

#include "SculptorCoreTypes.h"
#include "IESProfileTypes.h"


namespace spt::as
{

IESProfileDefinition ImportIESProfileFromString(lib::StringView iesData);

} // spt::as