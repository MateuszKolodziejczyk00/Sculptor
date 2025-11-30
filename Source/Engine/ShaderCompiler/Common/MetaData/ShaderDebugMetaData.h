#pragma once

#include "SculptorCoreTypes.h"
#include "Serialization.h"


namespace spt::sc
{

struct ShaderDebugMetaData
{
	lib::DynamicArray<lib::HashedString> literals;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("Literals", literals);
	}
};

} // spt::sc