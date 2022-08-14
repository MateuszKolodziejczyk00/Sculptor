#pragma once

#include "SculptorCoreTypes.h"
#include "yaml-cpp/emitter.h"

namespace YAML
{

inline Emitter& operator<<(Emitter& emitter, spt::lib::HashedString hashedString)
{
	return emitter.Write(hashedString.ToString());
}

} // YAML
