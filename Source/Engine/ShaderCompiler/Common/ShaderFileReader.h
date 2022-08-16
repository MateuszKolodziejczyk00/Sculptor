#pragma once

#include "SculptorCoreTypes.h"


namespace spt::sc
{

class ShaderFileReader
{
public:

	static lib::String			ReadShaderFileRelative(const lib::String& shaderRelativePath);

	static lib::String			ReadShaderFileAbsolute(const lib::String& shaderAbsolutePath);
};

}