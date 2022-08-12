#include "ShaderFileReader.h"
#include "ShaderCompilationEnvironment.h"

#include <fstream>
#include <sstream>

namespace spt::sc
{

lib::String ShaderFileReader::ReadShaderFile(const lib::HashedString& shaderRelativePath)
{
	SPT_PROFILE_FUNCTION();

	lib::String code;
	
	const lib::String shaderPath = ShaderCompilationEnvironment::GetShadersPath() + "/" + shaderRelativePath.ToString();

	std::ifstream stream(shaderPath);
	if (stream)
	{
		std::ostringstream stringStream;
		stringStream << stream.rdbuf();
		code = stringStream.str();
	}

	return code;
}

}
