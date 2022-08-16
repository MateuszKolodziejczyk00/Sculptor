#include "ShaderFileReader.h"
#include "ShaderCompilationEnvironment.h"

#include <fstream>
#include <sstream>

namespace spt::sc
{

lib::String ShaderFileReader::ReadShaderFileRelative(const lib::String& shaderRelativePath)
{
	SPT_PROFILE_FUNCTION();
	
	const lib::String shaderPath = ShaderCompilationEnvironment::GetShadersPath() + "/" + shaderRelativePath;

	return ReadShaderFileAbsolute(shaderPath);
}

spt::lib::String ShaderFileReader::ReadShaderFileAbsolute(const lib::String& shaderAbsolutePath)
{
	lib::String code;

	std::ifstream stream(shaderAbsolutePath);
	if (stream)
	{
		std::ostringstream stringStream;
		stringStream << stream.rdbuf();
		code = stringStream.str();
	}

	return code;
}

}
