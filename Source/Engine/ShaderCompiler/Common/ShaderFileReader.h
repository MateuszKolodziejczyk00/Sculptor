#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderFileReader
{
public:

	static lib::String			ReadShaderFile(const lib::HashedString& shaderRelativePath);
};

}