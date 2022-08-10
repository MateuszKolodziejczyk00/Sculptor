#include "CompilationErrorsLogger.h"
#include "ShaderCompilationEnvironment.h"
#include "ShaderCompilationInput.h"

#include <filesystem>
#include <fstream>

namespace spt::sc
{

namespace priv
{

lib::String getShaderTypeName(ECompiledShaderType type)
{
	switch (type)
	{
	case spt::sc::ECompiledShaderType::None:		return "None";
	case spt::sc::ECompiledShaderType::Vertex:		return "Vertex";
	case spt::sc::ECompiledShaderType::Fragment:	return "Fragment";
	case spt::sc::ECompiledShaderType::Compute:		return "Compute";
	}

	SPT_CHECK_NO_ENTRY(); // Invalid shader type
	return lib::String();
}


class LoggerStreamGuard
{
public:
	
	explicit LoggerStreamGuard(const lib::String& filePath)
	{
		m_stream.open(filePath, std::ios::trunc);
	}

	~LoggerStreamGuard()
	{
		m_stream.close();
	}

	std::ofstream& getStream()
	{
		return m_stream;
	}

private:

	std::ofstream		m_stream;
};

}

void CompilationErrorsLogger::OutputShaderPreprocessedCodeCode(const ShaderSourceCode& sourceCode)
{
	SPT_PROFILE_FUNCTION();

	const lib::String logFilePath = GetShaderPreprocessedCodeLogsPath(sourceCode);
	priv::LoggerStreamGuard streamGuard(logFilePath);

	std::ofstream& stream = streamGuard.getStream();

	stream << sourceCode.GetSourceCode();
}

lib::String CompilationErrorsLogger::GetShaderLogsPath(const ShaderSourceCode& sourceCode)
{
	const lib::String& logsPath = ShaderCompilationEnvironment::GetErrorLogsPath();
	SPT_CHECK(std::filesystem::is_directory(logsPath));

	return logsPath + "/" + sourceCode.GetName().ToString() + "/" + priv::getShaderTypeName(sourceCode.GetType());
}

lib::String CompilationErrorsLogger::GetShaderPreprocessedCodeLogsPath(const ShaderSourceCode& sourceCode)
{
	return GetShaderLogsPath(sourceCode) + "preprocessed.glsl";
}

}
